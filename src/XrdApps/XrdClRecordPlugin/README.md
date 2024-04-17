# 1 XrdClRecorder Plugin

This XRootD Client Plugin can be used to record all user's actions on XrdCl::File object and store them into a csv file. Afterwards, using the xrdreplay utility the actions can be replayed preserving the original timing.
The output file can be provided either using the `XRD_RECORDERPATH` environment variable or the `output` key in the plug-in configuration file (the enviroment variable takes precedence). If neither is provided the recorded actions will be stored at a default location: `/tmp/xrdrecord.csv`.


Config file format:

**recorder.conf:**

```bash
url = *
lib = /usr/lib64/libXrdClRecorder-5.so
enable = true
output = /tmp/out.csv # optional
```

In order to replay either do:

```bash
xrdreplay /tmp/out.csv
```

or

```bash
cat /tmp/out.csv | xrdreplay
```
_________________

# 2 xrdreplay tool

The **xrdreplay** application provides the following operation modes:
* <em>print</em> mode (-p)           : display runtime and IO statistics for a record file
* <em>verify</em> mode (-v)          : verify the existence of the required input files for a record file
* <em>creation</em> mode (-c,-t)     : create the required input data using file creation and write the minimal required size (-c) or truncate files to the minimal required size (-t)
* <em>playback</em> mode (default)   : replay a given record file

_________________

## 2.1 <em>print</em> mode (-p)

To display the IO statistics from a recorded file without playback do:

```bash
xrdreplay -p recording.csv

# =============================================
# IO Summary (print mode)
# =============================================
# Sampled Runtime  : 5.485724 s
# Playback Speed   : 1.00
# IO Volume (R)    : 536.87 MB [ std:536.87 MB vec:0 B page:0 B ] 
# IO Volume (W)    : 536.87 MB [ std:536.87 MB vec:0 B page:0 B ] 
# IOPS      (R)    : 64 [ std:64 vec:0 page:0 ] 
# IOPS      (W)    : 64 [ std:64 vec:0 page:0 ] 
# Files     (R)    : 1
# Files     (W)    : 1
# Datasize  (R)    : 536.87 MB
# Datasize  (W)    : 536.87 MB
# ---------------------------------------------
# Quality Estimation
# ---------------------------------------------
# Synchronicity(R) : 4.55%
# Synchronicity(W) : 100.00%
```

To further inspect details of the recording file (which files are accessed and IO per file) use the long format (-l) and optionally the summary option (-s):

```bash
xrdreplay -p recording.csv

# -----------------------------------------------------------------
# File: root://cmsserver//store/cms/higgs.root
# Sync: 4.55%
# Errs: 0
# -----------------------------------------------------------------
#     close::texec :             0.00 s
#     close::tnomi :             0.00 s
#      open::texec :             0.00 s
#      open::tnomi :             0.13 s
#      read::texec :             0.04 s
#      read::tnomi :            17.02 s
#      stat::texec :             0.00 s
#      stat::tnomi :             0.00 s
#         close::n :                1
#          open::n :                1
#         openr::n :                1
#          read::b :        536870912
#          read::n :               64
#          read::o :        536870912
#          stat::n :                1
# -----------------------------------------------------------------
...
```

All tags used in the output format are explained in the <em>tags</em> section.

_________________

## 2.2 <em>verify</em> mode (-v)

To verify the availability of all input files one uses:

```bash
xrdreplay -v recording.cvs

...
# ---------------------------------------------
# Verifying Dataset ...
# .............................................
# file: root://cmsserver//store/cms/higgs.root
# size: 536.87 MB [ 0 B out of 536.87 MB ]  ( 0.00% )
# ---> info: file exists and has sufficient size
```
On success the shell returns 0, if there was a missing, too small or inaccessible file it returns -5 (251).

```bash
Warning: xrdreplay considers a file only as an input file if it has no bytes written.
```
_________________

## 2.3 <em>creation</em> mode (-c)

In creation mode **xrdreplay** will create the required input files. In this context it is worthwhile to explain the <em>--replace</em> option, which allows to modify the input and output path used by **xrdreplay**.

### 2.3.1 using the --replace option
You can use the <em>--replace</em> option (multiple times) to rewrite the URLs of input and output data e.g.:

```bash
xrdreplay --replace root://cmsserver//store/cms/:=root://mycluster//mypath/ --replace file:/data/:=file:/gpfs/data/ -v

```
The option works in combination with all modes of **xrdreplay** e.g. you can create the required input files in a different location than given in the record file.

There are two ways to create input data:
* <em>-c</em> create files and write well defined patterns into the files to the minimum required offset given by the recorded pattern
* <em>-t</em> create files and truncate files to the minum required offset given by the recorded pattern (files will contain 0)

The truncate option might be not good to produce useful results when the storage systems supports sparse files and/or compression.

Her is an example to create input data in <em>create</em> mode in a modified storage endpoint:
```bash
xrdreplay --replace root://cmsserver//store/cms/:=root://mycluster//mypath/ -c
```

_________________

## 2.4 <em>playback</em> mode (default)
**xrdreplay** without print, verify or creation option will playback a recorded pattern file. By default **xrdreplay** will replay the IO trying to keep the original timing of each request. This might not be possible if responses are slower than in the original recording. It is possible to modify the playback speed using the <em>-x speedval</em> option. A value of 2 means to try to run the recorded pattern with double speed. A value of 0.5 means to replay the pattern at half speed. Increasing the playback speed can increase memory requirements significantly.

The playback mode creates some additional output lines:

```bash
xrdreplay recording.cvs

# =============================================
# IO Summary
# =============================================
# Total   Runtime  : 5.488581 s
# Sampled Runtime  : 5.485724 s
# Playback Speed   : 1.00
# IO Volume (R)    : 536.87 MB [ std:536.87 MB vec:0 B page:0 B ] 
# IO Volume (W)    : 536.87 MB [ std:536.87 MB vec:0 B page:0 B ] 
# IOPS      (R)    : 64 [ std:64 vec:0 page:0 ] 
# IOPS      (W)    : 64 [ std:64 vec:0 page:0 ] 
# Files     (R)    : 1
# Files     (W)    : 1
# Datasize  (R)    : 536.87 MB
# Datasize  (W)    : 536.87 MB
# IO BW     (R)    : 97.82 MB/s
# IO BW     (W)    : 97.82 MB/s
# ---------------------------------------------
# Quality Estimation
# ---------------------------------------------
# Performance Mark : 99.95%
# Gain Mark(R)     : 86.69%
# Gain Mark(W)     : 98.29%
# Synchronicity(R) : 4.55%
# Synchronicity(W) : 100.00%
# ---------------------------------------------
# Response Errors  : 0
# =============================================
```
Most of the output fields are self-explaining. <em>Performance Mark</em> puts the original run-time to the achieved run-time into relation.
The <Gain Marks> indicates if the IO could potentially be run faster than given by the recording (when > 100%). The <em>Synchronicity</em> measures the amount of IO requests within a given file are overlapping between request and response. A value of 100% indicates synchronous IO, a value towards 0 indicates asynchronous IO. This value does not measure parallelism between files.

In case of IO errors you will see a response error counter != 0 and a shell return code of -5 (251).
```bash
# ---------------------------------------------
# Response Errors  : 67
# =============================================
```

### 2.4.1 using the force (error suppression) mode (-f)
By default **xrdreplay** will reject to replay a recording file with error responses. By using the <em>-f</em> flag you can force the player to run. In this case unsuccessful IO events will be skipped in the replay.

_________________

## 2.5 <em>json</em> output
The print and playback modes support <em>json</em> output of results.

```bash
{
  "iosummary": { 
    "player::runtime": 7.8835,
    "player::speed": 1,
    "sampled::runtime": 5.48572,
    "volume::totalread": 536870912,
    "volume::totalwrite": 536870912,
    "volume::read": 536870912,
    "volume::write": 536870912,
    "volume::pgread": 0,
    "volume::pgwrite": 0,
    "volume::vectorread": 0,
    "volume::vectorwrite": 0,
    "iops::read": 64,
    "iops::write": 64,
    "iops::pgread": 0,
    "iops::pgwrite": 0,
    "iops::vectorread": 0,
    "iops::vectorwrite": 0,
    "files::read": 1,
    "files::write": 1,
    "bandwidth::MB::read": 68.1005,
    "bandwdith::MB::write": 68.1005,
    "performancemark": 69.5849,
    "gain::read":9.94212,
    "gain::write":42.2262
    "synchronicity::read":4.54545,
    "synchronicity::write":100,
    "response::error:":0
  }
}
```

Also the <em>-l</em> and <em>-s</em> options support <em>json</em> output.

_________________

## 2.6 command line usage

```
usage: xrdreplay [-p|--print] [-c|--create-data] [t|--truncate-data] [-l|--long] [-s|--summary] [-h|--help] [-r|--replace <arg>:=<newarg>] [-f|--suppress] [-v|--verify] [-x|--speed <value] p<recordfilename>]

                -h | --help             : show this help
                -f | --suppress         : force to run all IO with all successful result status - suppress all others
                                          - by default the player won't run with an unsuccessful recorded IO

                -p | --print            : print only mode - shows all the IO for the given replay file without actually running any IO
                -s | --summary          : print summary - shows all the aggregated IO counter summed for all files
                -l | --long             : print long - show all file IO counter for each individual file
                -v | --verify           : verify the existence of all input files
                -x | --speed <x>        : change playback speed by factor <x> [ <x> > 0.0 ]
                -r | --replace <a>:=<b> : replace in the argument list the string <a> with <b> 
                                          - option is usable several times e.g. to change storage prefixes or filenames

             [recordfilename]          : if a file is given, it will be used as record input otherwise STDIN is used to read records!
example:        ...  --replace file:://localhost:=root://xrootd.eu/        : redirect local file to remote
```

_________________

## 2.7 xrdreplay output tags

Per file (-l) or aggregated statistics (-s):

| tag                      | description                          |
| ------------------------ | ------------------------------------ |
| `*::texec`                 | playback execution time of * in sec  |
| `*::tnomi`                 | recorded execution time of * in sec  |
| `*::tloss`                 | time running after recording in sec  |
| `*::tgain`                 | time gained vs recording in sec      |
| `*::n`                     | number of IO calls to *              |
| `*::b`                     | sum of bytes for IO calls to *       |
| `*::o`                     | highest file offset accessed         |

When <em>*</em> is listed, this can be any allowed operation like <em>open,close,read,write,truncate,stat,sync,pgread,pgwrite,vectoread,vectorwrite</em>.

General output:

| tag                      | description                                | json tag                                     |
| ------------------------ | ------------------------------------------ | -------------------------------------------- |
| `Total Runtime`            | runtime of the replayed recording          | `player::runtime`                              | 
| `Sampled Runtime`          | original run time of the recording         | `sampled::runtime`                             |
| `Playback Speed`           | timestretch factor for the playback        | `player::speed`                                |
| `IO Volume (R)`            | total data INGRESS (by read func)          | `volume::totalread,read,pgread,vectoread`      |
| `IO Volume (W)`            | total data EGRESS  (by write func)         | `volume::totalwrite,write,pgwrite,vectorwrite` |
| `IOPS (R)`                 | total read IO ops  (by read func)          | `iops::read,pgread,vectoread`                  | 
| `IOPS (W)`                 | total write IO ops (by write func)         | `iops::write,pgwrite,vectorwrite`              |
| `Files (R)`                | sum files with reads                       | `files::read`                                  |
| `Files (W)`                | sum files with writes                      | `files::write`                                 |
| `Datasize (R)`             | sum of max file offsets in reads           | `datasetsize::read`                            |
| `Datasize (W)`             | sum of max file offsets in writes          | `datasetsize::write`                           |
| `IO BW (R)`                | bandwidth in MB/s from runtime for reading | `bandwidth::mb::read`                          |
| `IO BW (W)`                | bandwidth in MB/s from runtimg for writing | `bandwidth::mb::write`                         |
| `Performance Mark`         | 100 * (sampled runtime) / (total runtime)  | `performancemark`                              |
| `Gain Mark (R)`            | 100 * (sampled read times) / (total rtime) | `gain::read`                                   |
| `Gain Mark (W)`            | 100 * (sampled write times) / (total wtime)| `gain::write`                                  |
| `Synchronicity (R)`        | 100=sync 1=async IO for reads              | `synchronicity::read`                          |
| `Syncrhonicity (W)`        | 100=sync 1=async IO for writes             | `synchronicity::write`                         |
| `Response Errors`          | number of IOs which were not successfull   | `response::error`                              |

_________________

## 2.8 Memory consumption

It is possible to limit the memory consumption of xrdreplay by setting the `XRD_MAXBUFFERSIZE` environment variable, following sufixes are supported: kb, mb, gb (case insensitive).



