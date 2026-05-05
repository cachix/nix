#ifndef NIX_API_STORE_REALISATION_H
#define NIX_API_STORE_REALISATION_H
/**
 * @defgroup libstore_realisation Realisation
 * @ingroup libstore
 * @brief Realisation operations
 *
 * A realisation is the recorded result of building one output of a
 * content-addressed derivation: it maps a derivation output identifier
 * to the store path that build actually produced, along with any
 * signatures attesting to that mapping.
 * @{
 */
/** @file
 * @brief Realisation operations
 */

#include "nix_api_util.h"
#include "nix_api_store/fwd.h"
#include "nix_api_store/store_path.h"

#ifdef __cplusplus
extern "C" {
#endif
// cffi start

/** @brief Nix Realisation */
typedef struct nix_realisation nix_realisation;

/**
 * @brief Look up the realisation for a derivation output.
 *
 * The lookup first checks the exact derivation output id. If that misses
 * and the id names a derivation that Nix can resolve through known input
 * realisations, the lookup follows that resolution and returns the
 * realisation for the resolved derivation output.
 *
 * @param[out] context Optional, stores error information
 * @param[in] store nix store reference
 * @param[in] drv_output_id The derivation output identifier in the form
 *            `<drvPath>^<outputName>` where `drvPath` is the full
 *            store-dir-prefixed path to the derivation
 *            (e.g. `/nix/store/abc...-foo.drv^out`). Prefer building
 *            these via `nix_derivation_get_outputs`; the textual
 *            format may evolve.
 * @return A new realisation, or NULL. NULL has two meanings: either the
 *         realisation is not known to the store, or an error occurred.
 *         Disambiguate by checking `nix_err_code(context)`: `NIX_OK`
 *         means the realisation is missing; any other value means an
 *         error. Free the returned value with `nix_realisation_free`.
 */
nix_realisation *
nix_store_query_realisation(nix_c_context * context, Store * store, const char * drv_output_id);

/**
 * @brief Deallocate a `nix_realisation`.
 *
 * Does not fail. Calling with NULL is a no-op.
 *
 * @param[in] r the realisation to free
 */
void nix_realisation_free(nix_realisation * r);

/**
 * @brief Get the output store path recorded by a realisation.
 *
 * @param[out] context Optional, stores error information
 * @param[in] r the realisation
 * @return A new owned `StorePath`, or NULL on error. Free with
 *         `nix_store_path_free`.
 */
StorePath * nix_realisation_get_out_path(nix_c_context * context, const nix_realisation * r);

/**
 * @brief Enumerate the signatures attached to a realisation.
 *
 * Each signature is in the wire form `<key-name>:<signature-in-Base64>`
 * and can be re-parsed with the matching Nix signature parser.
 *
 * @note The callback may set an error on `context` to abort iteration
 * early; the surrounding call returns that error code.
 *
 * @param[out] context Optional, stores error information
 * @param[in] r the realisation
 * @param[in] userdata Arbitrary data passed to the callback
 * @param[in] callback Invoked once per signature, in unspecified order.
 *            The `signature` pointer is borrowed for the duration of
 *            the call only.
 */
nix_err nix_realisation_get_signatures(
    nix_c_context * context,
    const nix_realisation * r,
    void * userdata,
    void (*callback)(nix_c_context * context, void * userdata, const char * signature));

// cffi end
#ifdef __cplusplus
}
#endif
/**
 * @}
 */
#endif // NIX_API_STORE_REALISATION_H
