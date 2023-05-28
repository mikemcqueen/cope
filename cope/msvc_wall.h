#ifndef INCLUDE_MSVC_WALL_H
#define INCLUDE_MSVC_WALL_H

#if _MSC_VER // MSVC /Wall is too verbose. Make it quieter.
#pragma warning(disable:4514) // unreferenced inline function removed
#pragma warning(disable:4625) // copy ctor implicitly deleted
#pragma warning(disable:4626) // copy assignment oper implicitly deleted
//#pragma warning(disable:4668) // replacing undefined preprocessor macro with '0' for #if/#elif
#pragma warning(disable:4710) // function not inlined
#pragma warning(disable:4711) // function selected for inline expansion
#pragma warning(disable:4820) // struct padding notification
#pragma warning(disable:4868) // left-to-right evaluation not enforced in intializer-list
#pragma warning(disable:5026) // move ctor implicitly deleted
#pragma warning(disable:5027) // move assignment oper implicitly deleted
#pragma warning(disable:5045) // spectre mitigation notification
#endif // _MSC_VER

#endif // INCLUDE_MSVC_WALL_H