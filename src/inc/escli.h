#ifndef ESCLI_H
#define ESCLI_H


/** @brief */
typedef struct es_cli_s es_cli_t;

#if defined(__cplusplus)
extern "C" {
#endif

es_status es_cli_init(es_cli_t **ppCtx,struct event_base  *pBase);

es_status es_cli_start(es_cli_t *pCtx);

es_status es_cli_stop(es_cli_t *pCtx);

es_status es_cli_deinit(es_cli_t *pCtx);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif // ESCLI_H
