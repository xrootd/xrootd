# **CMS & WLCG XRootD Monitoring message structure** 

This document describes the information that we would like to obtain from XRootD servers to meet the monitoring goals of both WLCG and CMS.

Essentially, what is needed is an operation report that XRootD can generate once an operation has completed (whether successfully or not) and make available by any suitable means.

As a reference, the FTS events can be used as inspiration, a couple of examples are in @fts-examples.md

At present, we assume that most of this information can be derived by subscribing to the relevant monitoring streams and reconstructing the operation by stitching together the UDP packets. 

However, we would like to improve this model by assuming that we can obtain all necessary information in a single message emitted by XRootD. A couple of possible ways of doing that could be: 

• XRootD could provide all relevant information in the final event message (file\_close or an equivalent message for failed transfers and redirects).

• Alternatively, XRootD could produce a dedicated operation report in a separate stream once an operation concludes.

## **Required Fields**

Names are subject to change to match what’s already there.

| Name | Description | Comments | Stream |
| :---- | :---- | :---- | :---- |
| file\_name | The physical path to the file. |  OK | Map stream (d) |
| operation\_state | Status of the operation. | Successful / FailedIt’s not yet there |  |
| operation\_type | Type of operation. | Read / Write We currently infer this based on read/write bytes | F-stream (file close) |
| Node\_name / server\_name | The node that is executing and reporting the operation |   | Map stream (srvinfo) |
| ~~server\_name~~ | ~~The server reporting the operation~~ |  |  |
| client\_version | The client’s version | That’s a “nice to have”  | Map stream (appinfo?) |
| server\_ip | IP address of the server reporting the opreations. |   |  |
| server\_hostname | Hostname  of the server reporting the operations. | If resolvable. |  |
| server\_site | Source site  of the server reporting the opreations. | If available. Should be configured by the server | Map stream (srvinfo)  |
| client\_ip | IP address of the destination. |   | Map stream (part of the userid), could be an IP or a hostname |
| client\_hostname | Hostname of the destination. | If resolvable. | Map stream (part of the userid), could be an IP or a hostname |
| client\_site | Destination site. | If available. Should be configured by the client  | Map stream (appinfo? If configured) |
| auth\_method | Authentication method used. | token / x509 / none | Map stream (authinfo) |
| user | The DN or other identification of user | If applicable, DN for X509 maybe client\_id for token?  | Map stream (userid) |
| start\_time | Time when the operation started. |   | F-stream (FileTOD any event) |
| end\_time | Time when the operation concluded. |   | F-stream (FileTOD any event) |
| bytes | Number of bytes read or written. |   | F-stream (File close) |
| activity | Activity tag. | e.g. Analysis / Simulation / Production | Map stream (eainfo) |
| error\_message | Full error message. |   |  |
| error\_category | Error code or category. |   |  |
| vo | Virtual Organization (VO) associated with the operation. | Can be inferred from the token issuer if applicable. | Map stream (authinfo) |
| is\_local | Whether the operation is LAN or WAN traffic. | Xroot cannot do that. EOS is doing that for Alice (they are looking into the user info and filter out specific users) We do this manually. But there is some difference between local and internal. Local encompasses internal |  |
| ip\_version | IP protocol used for the operation. | IPv4 / IPv6 | Map stream (loginfo) |

 

Questions to be answered: 

Will the cgi information from the client make it to the streams?  
Aka if clients advertise a site name will this make it to the monitoring data? 
