# Example of a succesfull transfer

```json
{
  "_source": {
    "data": {
      "archiving": false,
      "archiving_finished": 0,
      "archiving_start": 0,
      "dest_se": "davs://cmsdcadisk.fnal.gov",
      "dst_url": "davs://cmsdcadisk.fnal.gov:2880/dcache/uscmsdisk/store/data/Run2025C/ParkingHH0/RAW/v1/000/392/925/00001/6527e5a6-0649-40a1-a7d0-a1836cb9e4e3.root",
      "endpnt": "fts3-cms.cern.ch",
      "file_id": 5188412381,
      "file_metadata": {
        "activity": "Production Input",
        "adler32": "a14e42c4",
        "dest_rse_id": "087ee3383b9d45f6b31814af07b2c56d",
        "dst_rse": "T1_US_FNAL_Disk",
        "dst_type": "DISK",
        "filesize": 2819158304,
        "md5": null,
        "name": "/store/data/Run2025C/ParkingHH0/RAW/v1/000/392/925/00001/6527e5a6-0649-40a1-a7d0-a1836cb9e4e3.root",
        "request_id": "64c8c447c69f45f497c79f8d8d539dc3",
        "request_type": "TRANSFER",
        "scope": "cms",
        "src_rse": "T2_CH_CERN",
        "src_rse_id": "542ab69d82bf401e9218bbe375bb1fce",
        "src_type": "DISK"
      },
      "file_state": "FINISHED",
      "fqdn": "fts-cms-002.cern.ch",
      "job_id": "153ceeb4-aa78-11f0-a217-fa163e6c8d57",
      "job_metadata": {
        "auth_method": "certificate",
        "issuer": "rucio",
        "multi_sources": false,
        "multihop": true,
        "overwrite_when_only_on_disk": false
      },
      "job_state": "FINISHED",
      "reason": "",
      "retry_counter": 0,
      "retry_max": 0,
      "source_se": "davs://eoscms.cern.ch",
      "src_url": "davs://eoscms.cern.ch:443/eos/cms/store/data/Run2025C/ParkingHH0/RAW/v1/000/392/925/00001/6527e5a6-0649-40a1-a7d0-a1836cb9e4e3.root",
      "staging": false,
      "staging_finished": 0,
      "staging_start": 0,
      "submit_time": 1760609318000,
      "timestamp": 1760620619125,
      "user_dn": "",
      "user_filesize": 2819158304,
      "user": "",
      "vo": "cms"
    },
}
```

# Example of a failed transfer

```json
{
  "_source": {
    "data": {
      "archiving": false,
      "archiving_finished": 0,
      "archiving_start": 0,
      "dest_se": "davs://mgm.hpc4l.org",
      "dst_url": "davs://mgm.hpc4l.org:8444/eos/cms/store/data/Run2025F/JetMET0/RAW/v1/000/397/332/00000/d2656d91-4110-4d87-b332-25bf9c121eb3.root",
      "endpnt": "fts3-cms.cern.ch",
      "file_id": 5182032775,
      "file_metadata": {
        "activity": "User AutoApprove",
        "adler32": "0d22dcf9",
        "dest_rse_id": "91f97b2defd64bc585b39ec61382f8ac",
        "dst_rse": "T2_LB_HPC4L",
        "dst_type": "DISK",
        "filesize": 5125174296,
        "md5": null,
        "name": "/store/data/Run2025F/JetMET0/RAW/v1/000/397/332/00000/d2656d91-4110-4d87-b332-25bf9c121eb3.root",
        "request_id": "0aa7b3c3657c40c9b4e3d2ebf6bdb931",
        "request_type": "TRANSFER",
        "scope": "cms",
        "src_rse": "T2_CH_CERN",
        "src_rse_id": "542ab69d82bf401e9218bbe375bb1fce",
        "src_type": "DISK"
      },
      "file_state": "FAILED",
      "fqdn": "fts-cms-005.cern.ch",
      "job_id": "b0d3923a-a8fa-11f0-bb0d-fa163e4409e3",
      "job_metadata": {
        "auth_method": "certificate",
        "issuer": "rucio",
        "multi_sources": false,
        "multihop": true,
        "overwrite_when_only_on_disk": false
      },
      "job_state": "FAILED",
      "reason": "TRANSFER [5] TRANSFER ERROR: Copy failed (3rd pull). Last attempt: Connection terminated abruptly; Status of TPC request unknown",
      "retry_counter": 0,
      "retry_max": 0,
      "source_se": "davs://eoscms.cern.ch",
      "src_url": "davs://eoscms.cern.ch:443/eos/cms/store/data/Run2025F/JetMET0/RAW/v1/000/397/332/00000/d2656d91-4110-4d87-b332-25bf9c121eb3.root",
      "staging": false,
      "staging_finished": 0,
      "staging_start": 0,
      "submit_time": 1760445511000,
      "timestamp": 1760620654106,
      "user_dn": "",
      "user_filesize": 5125174296,
      "user": "",
      "vo": "cms"
    },
    "metadata": {
      "offset": "653073888",
      "type": "state",
      "event_timestamp": 1760620654106,
      "version": "007",
      "hostname": "monit-amqsource-a02.cern.ch",
      "partition": "19",
      "type_prefix": "raw",
      "kafka_timestamp": 1760620665372,
      "topic": "monit-fts_raw_state",
      "producer": "fts",
      "_id": "8b1836fa-7cf2-623e-151d-cc22532679aa",
      "timestamp": 1760620664510
    }
  }
}
```
