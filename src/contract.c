/*
 * The C content contract - a technology of xmip-core-contract, in C.
 *
 * ADR-0042 decision 3: a contract may be authored in any declared language
 * over the C ABI, and this is the C one. ADR-0012: the header is the boundary,
 * so this file includes xmip_module.h and nothing of Xmip's Rust. It is the
 * reference for every other language here: the same entrypoint, the same
 * contract table, the same lifecycle.
 *
 * What it claims: well-formedness is bytes - a Stream this contract sees is
 * read to its end and held (ADR-0042 decision 1). A descriptor a Location
 * binds is kept and reported back by `implies` under the key "descriptor";
 * a C contract with a real standard replaces `judge` and nothing else.
 */

#include "xmip_module.h"

#include <stdlib.h>
#include <string.h>

/* The most a single diagnostic message may say. */
#define XMIP_C_MESSAGE_MAX 256

typedef struct {
    char   *descriptor;
    size_t  descriptor_len;
} Contract;

typedef struct {
    /* Diagnostics are borrowed until the next call, so they live here. */
    XmipDiagnostic diagnostic;
    char           message[XMIP_C_MESSAGE_MAX];
    char           error[XMIP_C_MESSAGE_MAX];
} State;

static XmipStr str_of(const char *text, size_t len) {
    XmipStr s;
    s.ptr = (const uint8_t *)text;
    s.len = len;
    return s;
}

/* Header section 8: lifecycle. Nothing to configure, start or stop here. */
static XmipStatus configure(void *state, XmipStr toml) { (void)state; (void)toml; return XMIP_OK; }
static XmipStatus start(void *state) { (void)state; return XMIP_OK; }
static XmipStatus stop(void *state) { (void)state; return XMIP_OK; }

static XmipStatus load(void *state, XmipStr descriptor, void **out_contract) {
    Contract *contract;
    (void)state;
    if (out_contract == NULL) return XMIP_E_INVALID;
    contract = (Contract *)calloc(1, sizeof *contract);
    if (contract == NULL) return XMIP_E_CAPACITY;
    if (descriptor.len > 0) {
        contract->descriptor = (char *)malloc(descriptor.len);
        if (contract->descriptor == NULL) { free(contract); return XMIP_E_CAPACITY; }
        memcpy(contract->descriptor, descriptor.ptr, descriptor.len);
        contract->descriptor_len = descriptor.len;
    }
    *out_contract = contract;
    return XMIP_OK;
}

static void release(void *state, void *contract) {
    Contract *c = (Contract *)contract;
    (void)state;
    if (c == NULL) return;
    free(c->descriptor);
    free(c);
}

/*
 * Judge the bytes read so far. The identity contract holds everything; a
 * contract with a standard replaces this function. Return 0 to hold, or fill
 * the diagnostic and return its code.
 */
static XmipStatus judge(State *s, const Contract *c, const uint8_t *bytes, size_t len) {
    (void)s; (void)c; (void)bytes; (void)len;
    return XMIP_OK;
}

static XmipStatus validate(void *state, void *contract, const XmipReader *in,
                           const XmipDiagnostic **out, size_t *out_len) {
    State *s = (State *)state;
    uint8_t *bytes = NULL;
    size_t len = 0, cap = 0;
    XmipStatus verdict;
    if (s == NULL || in == NULL || in->read == NULL || out == NULL || out_len == NULL) {
        return XMIP_E_INVALID;
    }
    *out = NULL;
    *out_len = 0;
    for (;;) {
        int64_t got;
        if (cap - len < 4096) {
            size_t grown = cap == 0 ? 8192 : cap * 2;
            uint8_t *bigger = (uint8_t *)realloc(bytes, grown);
            if (bigger == NULL) { free(bytes); return XMIP_E_CAPACITY; }
            bytes = bigger;
            cap = grown;
        }
        got = in->read(in->ctx, bytes + len, cap - len);
        if (got < 0) { free(bytes); return (XmipStatus)got; }
        if (got == 0) break;
        len += (size_t)got;
    }
    verdict = judge(s, (const Contract *)contract, bytes, len);
    free(bytes);
    if (verdict != XMIP_OK) {
        *out = &s->diagnostic;
        *out_len = 1;
    }
    return verdict;
}

static XmipStatus implies(void *state, void *contract, XmipStr key, XmipStr *out) {
    const Contract *c = (const Contract *)contract;
    (void)state;
    if (c == NULL || out == NULL) return XMIP_E_INVALID;
    if (key.len == 10 && memcmp(key.ptr, "descriptor", 10) == 0 && c->descriptor_len > 0) {
        *out = str_of(c->descriptor, c->descriptor_len);
        return XMIP_OK;
    }
    return XMIP_E_NOT_FOUND;
}

static XmipStr last_error(void *state) {
    State *s = (State *)state;
    return str_of(s->error, strlen(s->error));
}

static void destroy(void *state) { free(state); }

static const XmipContractVtable VTABLE = {
    { 1u, 0u, configure, start, stop },
    load, release, validate, implies
};

XMIP_EXPORT XmipStatus xmip_create_module_v1(const XmipHost *host, XmipModule *out) {
    State *state;
    if (host == NULL || out == NULL) return XMIP_E_INVALID;
    /* Section 7: a foreign abi_version is refused here, *out untouched. */
    if (host->abi_version != XMIP_ABI_VERSION) return XMIP_E_UNSUPPORTED;
    state = (State *)calloc(1, sizeof *state);
    if (state == NULL) return XMIP_E_CAPACITY;
    state->diagnostic.code = XMIP_E_CONTRACT;
    state->diagnostic.message = str_of(state->message, 0);
    state->diagnostic.location = str_of("", 0);
    state->diagnostic.offset = UINT64_MAX;

    out->descriptor.abi_version = XMIP_ABI_VERSION;
    out->descriptor.provider = str_of("core", 4);
    out->descriptor.module = str_of("contract", 8);
    out->descriptor.standard = str_of("c", 1);
    out->descriptor.trait_major = 1u;
    out->descriptor.trait_minor = 0u;
    out->descriptor.module_major = 0u;
    out->descriptor.module_minor = 1u;
    out->descriptor.module_patch = 0u;
    out->state = state;
    out->vtable = &VTABLE;
    out->last_error = last_error;
    out->destroy = destroy;
    return XMIP_OK;
}
