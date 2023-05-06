#include "XrdCephBulkAioRead.hh"


bulkAioRead::bulkAioRead(librados::IoCtx* ct, logfunc_pointer logwrapper, CephFileRef* fileref) {
  /**
   * Constructor.
   *
   * @param ct                Rados IoContext object
   * @param logfunc_pointer   Pointer to the function that will be used for logging
   * @param fileref           Ceph file reference
   *
   */
  context = ct;
  file_ref = fileref;
  log_func = logwrapper;
}

bulkAioRead::~bulkAioRead() {
  /**
   * Destructor. Just clears dynamically allocated memroy.
   */
  clear();
}

void bulkAioRead::clear() {
  /**
   * Clear all dynamically alocated memory
   */
  operations.clear();
  buffers.clear();
}

int bulkAioRead::addRequest(size_t obj_idx, char* out_buf, size_t size, off64_t offset) {
  /**
   * Prepare read request for a single ceph object. Private method.
   *
   * Method will allocate all (well, almost, except the string for the object name)
   * necessary objects to submit read request to ceph. To submit the requests use
   * `submit_and_wait_for_complete` method.
   *
   * @param obj_idx  number of the object (starting from zero) to read
   * @param out_buf  output buffer, where read results should be stored
   * @param size     number of bytes to read
   * @param offset   offset in bytes where the read should start. Note that the offset is local to the
   *                 ceph object. I.e. if offset is 0 and object number is 1, yo'll be reading from the
   *                 start of the second object, not from the begining of the file.
   *
   * @return         zero on success, negative error code on failure
   */

  try{
    auto &op_data = operations[obj_idx];
    //When we start using C++17, the next two lines can be merged
    buffers.emplace_back(out_buf);
    auto &buf = buffers.back();
    op_data.ceph_read_op.read(offset, size, &buf.bl, &buf.rc);
  } catch (std::bad_alloc&) {
   log_func((char*)"Memory allocation failed while reading file %s", file_ref->name.c_str());
   return -ENOMEM;
  }
  return 0;
}

int bulkAioRead::submit_and_wait_for_complete() {
  /**
   * Submit previously prepared read requests and wait for their completion
   *
   * To prepare read requests use `read` or `addRequest` methods.
   *
   * @return  zero on success, negative error code on failure
   *
   */

  for (auto &op_data: operations) {
    size_t obj_idx = op_data.first;
    //16 bytes for object hex number, 1 for dot and 1 for null-terminator
    char object_suffix[18];
    int sp_bytes_written;
    sp_bytes_written = snprintf(object_suffix, sizeof(object_suffix), ".%016zx", obj_idx);
    if (sp_bytes_written >= (int) sizeof(object_suffix)) {
      log_func((char*)"Can not fit object suffix into buffer for file %s -- too big\n", file_ref->name.c_str());
      return -EFBIG;
    }

    std::string obj_name;
    try {
      obj_name =  file_ref->name + std::string(object_suffix);
    } catch (std::bad_alloc&) {
      log_func((char*)"Can not create object string for file %s)", file_ref->name.c_str());
      return -ENOMEM;
    }
    context->aio_operate(obj_name, op_data.second.cmpl.use(), &op_data.second.ceph_read_op, 0);
  }

  for (auto &op_data: operations) {
    op_data.second.cmpl.wait_for_complete();
    int rval = op_data.second.cmpl.get_return_value();
    /*
     * Optimization is possible here: cancel all remaining read operations after the failure.
     * One way to do so is the following: add context as an argument to the `use` method of CmplPtr.
     * Then inside the class this pointer can be saved and used by the destructor to call
     * `aio_cancel` (and probably `wait_for_complete`) before releasing the completion.
     * Though one need to clarify whether it is necessary to cal `wait_for_complete` after
     * `aio_cancel` (i.e. may the status variable/bufferlist still be written to or not).
     */
    if (rval < 0) {
      log_func((char*)"Read of the object %ld for file %s failed", op_data.first, file_ref->name.c_str());
      return rval;
    }
  }
  return 0;
}

ssize_t bulkAioRead::get_results() {
  /**
   * Copy the results of executed read requests from ceph's bufferlists to client's buffers
   *
   * Note that this method should be called only after the submission and completion of read
   * requests, i.e. after `submit_and_wait_for_complete` method.
   *
   * @return  cumulative number of bytes read (by all read operations) on success, negative
   *          error code on failure
   *
   */

  ssize_t res = 0;
  for (ReadOpData &op_data: buffers) {
    if (op_data.rc < 0) {
      //Is it possible to get here?
      log_func((char*)"One of the reads failed with rc %d", op_data.rc);
      return op_data.rc;
    }
    op_data.bl.begin().copy(op_data.bl.length(), op_data.out_buf);
    res += op_data.bl.length();
  }
  //We should clear used completions to allow new operations
  clear();
  return res;
}

int bulkAioRead::read(void* out_buf, size_t req_size, off64_t offset) {
  /**
   * Declare a read operation for file.
   *
   * Read coordinates are global, i.e. valid offsets are from 0 to the <file_size> -1, valid request sizes
   * are from 0 to INF. Method can be called multiple times to declare multiple read
   * operations on the same file.
   *
   * @param out_buf    output buffer, where read results should be stored
   * @param req_size   number of bytes to read
   * @param offset     offset in bytes where the read should start. Note that the offset is global,
   *                   i.e. refers to the whole file, not individual ceph objects
   *
   * @return  zero on success, negative error code on failure
   *
   */

  if (req_size == 0) {
    log_func((char*)"Zero-length read request for file %s, probably client error", file_ref->name.c_str());
    return 0;
  }

  char* const buf_start_ptr = (char*) out_buf;

  size_t object_size = file_ref->objectSize;
  //The amount of bytes that is yet to be read
  size_t to_read = req_size;
  //block means ceph object here
  size_t start_block = offset / object_size;
  size_t buf_pos = 0;
  size_t chunk_start = offset % object_size;

  while (to_read > 0) {
    size_t chunk_len = std::min(to_read, object_size - chunk_start);

    if (buf_pos >= req_size) {
      log_func((char*)"Internal bug! Attempt to read %lu data for block (%lu, %lu) of file %s\n", buf_pos, offset, req_size, file_ref->name.c_str());
      return -EINVAL;
    }

    int rc = addRequest(start_block, buf_start_ptr + buf_pos, chunk_len, chunk_start);
    if (rc < 0) {
      log_func((char*)"Unable to submit async read request, rc=%d\n", rc);
      return rc;
    }

    buf_pos += chunk_len;
    
    start_block++;
    chunk_start = 0;
    if (chunk_len > to_read) {
      log_func((char*)"Internal bug! Read %lu bytes, more than expected %lu bytes for block (%lu, %lu) of file %s\n", chunk_len, to_read, offset, req_size, file_ref->name.c_str());
      return -EINVAL;
    }
    to_read = to_read - chunk_len;
  }
  return 0;
}
