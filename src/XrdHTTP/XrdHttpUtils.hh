/** @file  XrdHttpUtils.hh
 * @brief  Utility functions for XrdHTTP
 * @author Fabrizio Furano
 * @date   April 2013
 * 
 * 
 * 
 */



#ifndef XRDHTTPUTILS_HH
#define	XRDHTTPUTILS_HH


// GetHost from URL
// Parse an URL and extract the host name and port
// Return 0 if OK
int parseURL(char *url, char *host, int &port, char **path);


void calcHashes(
        char *hash,

        const char *fn,

        kXR_int16 req,

        XrdSecEntity *secent,
        
        time_t tim,

        const char *key);


int compareHash(
        const char *h1,
        const char *h2);



// Create a new quoted string
char *quote(char *str);

// unquote a string and return a new one
char *unquote(char *str);

#endif	/* XRDHTTPUTILS_HH */

