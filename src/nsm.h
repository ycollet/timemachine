#ifndef NSM_H
#define NSM_H

#ifdef HAVE_LIBLO
int  nsm_init(const char *app_name, const char *executable);
void nsm_free(void);
#endif

#endif
