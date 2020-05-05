#pragma once
/* Software License Agreement
 *
 *     Copyright(C) 1994-2020 David Lindauer, (LADSoft)
 *
 *     This file is part of the Orange C Compiler package.
 *
 *     The Orange C Compiler package is free software: you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation, either version 3 of the License, or
 *     (at your option) any later version.
 *
 *     The Orange C Compiler package is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 *
 *     You should have received a copy of the GNU General Public License
 *     along with Orange C.  If not, see <http://www.gnu.org/licenses/>.
 *
 *     contact information:
 *         email: TouchStone222@runbox.com <David Lindauer>
 *
 */
#include "ctypes.h"

typedef struct _memblk_
{
    struct _memblk_* next;
    long size;
    long left;
    char m[1]; /* memory area */
} MEMBLK;
void memfunc(const char *a);
void mem_summary(void);
void* memAlloc(MEMBLK** arena, int size, bool clear = true);
void memFree(MEMBLK** arena, int* peak);
void* globalAlloc(int size);
void globalFree(void);
void* localAlloc(int size);
void localFree(void);
void* Alloc(int size);
void* nzAlloc(int size);
void* oAlloc(int size);
void oFree(void);
void* aAlloc(int size);
void aFree(void);
void* tAlloc(int size);
void tFree(void);
void* cAlloc(int size);
void cFree(void);
void* sAlloc(int size);
void sFree(void);
void SetGlobalFlag(bool flag, bool &old);
void ReleaseGlobalFlag(bool old);
bool GetGlobalFlag(void);
char* litlate(const char* name);
LCHAR* wlitlate(const LCHAR* name);