#ifndef JAIL_DATATYPES_H
#define JAIL_DATATYPES_H

#include "Jail.h"

namespace JAIL {

    extern void registerChar(JInterpreter *interpreter);
    extern void registerInteger(JInterpreter *interpreter);
    extern void registerDouble(JInterpreter *interpreter);
    extern void registerString(JInterpreter *interpreter);
    extern void registerArray(JInterpreter *interpreter);
    extern void registerObject(JInterpreter *interpreter);

};

#endif