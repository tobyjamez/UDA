#ifndef IDAM_PUTGROUP_H
#define IDAM_PUTGROUP_H

#include <idamplugin.h>

int do_group(IDAM_PLUGIN_INTERFACE* idam_plugin_interface);
int testgroup(int ncgrpid, const char* target, int* status, int* targetid);

#endif //IDAM_PUTGROUP_H
