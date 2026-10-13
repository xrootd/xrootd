#ifndef __XRDOUCBEARERTOKEN_HH__
#define __XRDOUCBEARERTOKEN_HH__

#include <cstddef>
#include <string>
#include <string_view>

class XrdOucEnv;

/**
 * WLCG bearer-token discovery helpers shared by client components.
 *
 * Implements the discovery order defined by the WLCG Bearer Token Discovery
 * specification and currently used by the ztn security protocol.
 * https://github.com/WLCG-AuthZ-WG/bearer-token-discovery
 */
class XrdOucBearerToken {
   public:
    enum class Status { Found, NotFound, Error };

    struct Result {
        Status status{Status::NotFound};
        std::string token;
        int errnum{0};
        std::string location;
        // Discovery step that produced this result, such as "BEARER_TOKEN_FILE"
        // or "xrd.ztn". Set by Discover() for any result other than a miss of
        // the whole sequence.
        std::string entry;

        // Text for a discovery failure, including the step name and location.
        std::string Failure() const;
    };

    /**
     * Read a bearer token from a file.
     *
     * The file must be readable only by its owner. Group or world-accessible
     * files are rejected with EPERM.
     *
     * @param path    path to the token file
     * @param maxSize exclusive maximum stripped-token length in bytes, or 0 for
     *                no limit. A token of this length or longer is rejected with
     *                EMSGSIZE. A file larger than @p maxSize is rejected before
     *                it is read.
     * @return a Result with status
     *         Found and token set on success;
     *         NotFound if path does not exist or contains only whitespace;
     *         Error with errnum and location set on failure
     */
    static Result ReadFile(const char* path, size_t maxSize = 0);

    /**
     * Extract a bearer token from a raw environment value.
     *
     * @param value   token contents
     * @param maxSize exclusive maximum stripped-token length in bytes, or 0 for
     *                no limit
     * @return a Result with status Found and token set on success;
     *          NotFound if @p value is null, empty, or whitespace only;
     *          Error with errnum set to EMSGSIZE if the stripped token is
     *          @p maxSize bytes or longer. location is left empty so the token
     *          is not logged.
     */
    static Result FromEnvValue(const char* value, size_t maxSize = 0);

    /**
     * Resolve one WLCG bearer-token discovery entry.
     *
     * Supported forms match the ztn client lookup vector entries:
     *   - BEARER_TOKEN : read the variable value directly
     *   - BEARER_TOKEN_FILE : read the file named by the variable
     *   - XDG_RUNTIME_DIR : read $XDG_RUNTIME_DIR/bt_u<uid>
     *   - absolute paths containing %d : read the path with the effective user
     *     id substituted (for example /tmp/bt_u%d)
     *
     * @param entry discovery entry name or path pattern
     * @param maxSize exclusive maximum stripped-token length in bytes, or 0 for
     *                no limit
     * @return the Result from the resolved entry, or NotFound if @p entry is
     *         unknown or the entry is unset
     */
    static Result ResolveEntry(std::string_view entry, size_t maxSize = 0);

    /**
     * Run the default WLCG bearer-token discovery sequence.
     *
     * Locations are tried in order until one returns Found or Error:
     *   1. BEARER_TOKEN
     *   2. BEARER_TOKEN_FILE
     *   3. XDG_RUNTIME_DIR
     *   4. /tmp/bt_u<uid>
     *   5. @p xrdZtnPath, when provided
     *
     * @param maxSize    exclusive maximum stripped-token length in bytes, or 0
     *                   for no limit
     * @param xrdZtnPath optional path from the xrd.ztn opaque parameter
     * @return the first non-NotFound Result from the sequence, or NotFound if no
     *         token is available. Result::entry names the step that produced a
     *         Found or Error result, and "xrd.ztn" when that path is consulted.
     */
    static Result Discover(size_t maxSize = 0, const char* xrdZtnPath = nullptr);

    /**
     * Run discovery using the xrd.ztn path stored in @p env, when that
     * parameter is present.
     */
    static Result Discover(size_t maxSize, XrdOucEnv* env);
};

#endif
