#ifndef PORT_TAG_TEXT_H
#define PORT_TAG_TEXT_H
#ifdef __cplusplus
extern "C" {
#endif
/* UTF-8 presentation text only; never use for room packets or saved keys. */
const char *Port_TagText(const char *english);
#ifdef __cplusplus
}
#endif
#endif
