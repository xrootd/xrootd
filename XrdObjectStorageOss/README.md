# Windows instructions

Install WSL2 and Visual Studio 2022 beta.

Then follow instructions to enable CMake for the Linux subsystem [here](https://devblogs.microsoft.com/cppblog/build-and-debug-c-with-wsl-2-distributions-and-visual-studio-2022/)

Open a Linux terminal and install prerequisites:

```
sudo apt-get update -y
sudo apt-get install -y \
   g++ gdb make ninja-build rsync zip pkg-config libreadline-dev \
   libkrb5-dev libfuse-dev libcurl3-dev libxml2-dev libtinyxml-dev  \
   libsystemd-dev libcppunit-dev  libjson-c-dev
```

From `Project menu` select `Configure xrootd`:

```
1> [CMake] -- Could NOT find Macaroons (missing: MACAROONS_INCLUDES MACAROONS_LIB) 
1> [CMake] -- Checking for module 'json-c'
1> [CMake] --   No package 'json-c' found
1> [CMake] -- Could NOT find SciTokensCpp (missing: SCITOKENS_CPP_LIBRARIES SCITOKENS_CPP_INCLUDE_DIR) 
1> [CMake] -- Could NOT find PythonLibs (missing: PYTHON_LIBRARIES PYTHON_INCLUDE_DIRS) (Required is at least version "2.4")
1> [CMake] -- Could NOT find VOMS (missing: VOMS_LIBRARY VOMS_INCLUDE_DIR) 
...
1> [CMake] -- Readline support:  yes
1> [CMake] -- Fuse support:      yes
1> [CMake] -- Crypto support:    yes
1> [CMake] -- Kerberos5 support: yes
1> [CMake] -- XrdCl:             yes
1> [CMake] -- Tests:             yes
1> [CMake] -- HTTP support:      yes
1> [CMake] -- HTTP TPC support:  yes
1> [CMake] -- Macaroons support: disabled
1> [CMake] -- VOMS support:      disabled
1> [CMake] -- Python support:    disabled
```

Then switch the "Solution Explored View" to "CMake Target" View and `Build All`

# Test

See http://fmatrm.if.usp.br/cgi-bin/man/man2html?xrootd+1

Example for Windows WSL2:

```
cd ~/.vs/xrootd
export PATH=./out/build/linux-default/src:./out/build/linux-default/src/XrdCl/:./out/build/linux-default/src/XrdCl/:$PATH
SERVER_DIR=/tmp
XROOTD_SERVER=root://$(hostname --fqdn):8094/$SERVER_DIR
```

Create a normal configuration:

```
cat << EOF > /home/scrgiorgio/xrootd.config
xrd.port 8094
xrootd.export $SERVER_DIR
EOF

```

Create an object storage configuration:

```
cat << EOF > /home/scrgiorgio/xrootd.object_storage.config
xrd.port 8094
xrootd.export $SERVER_DIR
ofs.osslib /home/scrgiorgio/.vs/xrootd/out/build/linux-default/XrdObjectStorageOss/libXrdObjectStorageOss.so
xrdobjectstorageoss.connection_string https://bucket_name_here.s3.amazonaws.com?username=XXXXX&password=YYYYY
xrdobjectstorageoss.num_connections 8
EOF
```
Run the server:

```
xrootd -c /home/scrgiorgio/xrootd.config
xrootd -c /home/scrgiorgio/xrootd.object_storage.config
```

Use xrootd command utils:

```
# Copy a file
xrdcp $XROOTD_SERVER/nsdf.png .      

# List files 
xrdfs $XROOTD_SERVER ls $SERVER_DIR  
```



# XrootD as a FUSE file system

Example:

```
sudo nano /etc/fuse.conf
# enable the `user_allow_other` option by uncommenting the first # character from its line

LOCAL_DIR=/mnt/test
sudo mkdir -p $LOCAL_DIR
sudo chmod a+rwX -R $LOCAL_DIR
xrootdfs -d -o rdr=$XROOTD_SERVER,uid=daemon $LOCAL_DIR 
```
