#ifndef _SPECANNOTATION_API_H
#define _SPECANNOTATION_API_H

#define NEW_SIZE(type, size) (type*) malloc(size)
#define NEW(type) NEW_SIZE(type, sizeof(type))

struct MethodCall;
typedef struct MethodCall *Method;
typedef const char *CSTR;

struct AnnoInterfaceInfo;
typedef struct AnnoInterfaceInfo AnnoInterfaceInfo;

#ifdef __cplusplus
extern "C" {
#endif

struct AnnoInterfaceInfo* _createInterfaceBeginAnnotation(CSTR name);

void _setInterfaceBeginAnnotationValue(struct AnnoInterfaceInfo *info, void *value);

void _createInterfaceEndAnnotation(CSTR name);

void _createOPDefineAnnotation();

void _createPotentialOPAnnotation(CSTR label);

void _createOPCheckAnnotation(CSTR label);

void _createOPClearAnnotation();

void _createOPClearDefineAnnotation();

#ifdef __cplusplus
};
#endif

#endif
