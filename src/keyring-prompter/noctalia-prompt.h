#ifndef __NOCTALIA_PROMPT_H__
#define __NOCTALIA_PROMPT_H__

#define GCR_API_SUBJECT_TO_CHANGE 1
#include <gcr/gcr.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define NOCTALIA_TYPE_PROMPT (noctalia_prompt_get_type ())
G_DECLARE_FINAL_TYPE (NoctaliaPrompt, noctalia_prompt, NOCTALIA, PROMPT, GObject)

NoctaliaPrompt *noctalia_prompt_new (void);

G_END_DECLS

#endif /* __NOCTALIA_PROMPT_H__ */
