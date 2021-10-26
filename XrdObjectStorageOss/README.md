# Instructions

If you are on Windows, nstall WSL2 and Visual Studio 2022 beta.
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

Then `Build All`

# Test

See http://fmatrm.if.usp.br/cgi-bin/man/man2html?xrootd+1

Create a new `my_xrootd.config` file like this (changed as needed):

```
# the port
xrd.port 8094

# the directory to explort
xrootd.export /tmp

# where to find the object storage oss plugin
ofs.osslib /path/to/libXrdObjectStorageOss.so

# connection string 
xrdobjectstorageoss.connection_string https://s3.amazonaws.com/test-openvisus-bucket/teststorage?access_key=XXXX&secret_key=YYYYY

# num_connections
xrdobjectstorageoss.num_connections 8
````


Example of usage:

```
xrootd -c /mnt/d/GoogleDrive/.config/xrootd/aws.config # /tmp PORT 8094  
xrootd -c /mnt/d/GoogleDrive/.config/xrootd/osn.config # /tmp PORT 8095
xrootd -c /mnt/d/GoogleDrive/.config/xrootd/was.config # /tmp PORT 8096
```

Test/Use it:

```
# test getDir
xrdfs root://localhost:8094/ ls /tmp
xrdfs root://localhost:8094/ ls /tmp/
xrdfs root://localhost:8094/ ls /tmp/docs
xrdfs root://localhost:8094/ ls /tmp/docs/powerpoint

# test getBlob
xrdcp root://localhost:8094//tmp/docs/compilation.md $PWD

# test addBlob
xrdcp  $PWD/compilation.md                               root://localhost:8094//tmp/docs/compilation2.md
xrdcp  root://localhost:8094//tmp/docs/compilation2.md   $PWD/compilation3.md 


diff $PWD/compilation.md $PWD/compilation3.md

# test delBlob
xrdfs root://localhost:8094/ ls /tmp/docs
xrdfs root://localhost:8094/ rm /tmp/docs/compilation2.md


# XrootD as a FUSE file system
#   enable the `user_allow_other` option by uncommenting the first # character from its line
#   sudo nano /etc/fuse.conf
LOCAL_DIR=~/remove-me
sudo mkdir -p $LOCAL_DIR
sudo chmod a+rwX -R $LOCAL_DIR
xrootdfs -d -o rdr=root://localhost:8094/,uid=daemon $LOCAL_DIR 
```
