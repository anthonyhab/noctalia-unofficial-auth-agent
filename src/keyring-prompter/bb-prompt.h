#ifndef __BB_AUTH_PROMPT_H__
#define __BB_AUTH_PROMPT_H__

#define GCR_API_SUBJECT_TO_CHANGE 1
#include <gcr/gcr.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define BB_AUTH_TYPE_PROMPT (bb_auth_prompt_get_type ())
G_DECLARE_FINAL_TYPE (BbAuthPrompt, bb_auth_prompt, BB_AUTH, PROMPT, GObject)

BbAuthPrompt *bb_auth_prompt_new (void);

G_END_DECLS

#endif /* __BB_AUTH_PROMPT_H__ */
