/* Software License Agreement
 * 
 *     Copyright(C) 1994-2024 David Lindauer, (LADSoft)
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
 * 
 */

#include "compiler.h"
#include <stack>
#include "Utils.h"
#include "ccerr.h"
#include "declcpp.h"
#include "config.h"
#include "mangle.h"
#include "initbackend.h"
#include "occparse.h"
#include "templatedecl.h"
#include "templateutil.h"
#include "templateinst.h"
#include "templatededuce.h"
#include "lambda.h"
#include "lex.h"
#include "help.h"
#include "stmt.h"
#include "expr.h"
#include "rtti.h"
#include "constopt.h"
#include "OptUtils.h"
#include "memory.h"
#include "inline.h"
#include "init.h"
#include "beinterf.h"
#include "declcons.h"
#include "ildata.h"
#include "types.h"
#include "declare.h"
#include "constopt.h"
#include "constexpr.h"
#include "optmain.h"
#include "iexpr.h"
#include "iblock.h"
#include "istmt.h"
#include "iinline.h"
#include "namespace.h"
#include "symtab.h"
#include "ListFactory.h"
#include "inline.h"
#include "overload.h"
#include "class.h"

namespace Parser
{
int packIndex;

static bool hasPackedTemplate(Type* tp)
{

    switch (tp->type)
    {
        case BasicType::typedef_:
            break;
        case BasicType::aggregate_:
            tp = tp->syms->front()->tp;
            /* fall through */
        case BasicType::func_:
        case BasicType::ifunc_:
            if (hasPackedTemplate(tp->btp))
                return true;
            if (tp->syms)
            {
                for (auto sym : *tp->syms)
                {
                    if (hasPackedTemplate(sym->tp))
                        return true;
                }
            }
            break;
        case BasicType::float__complex_:
            break;
        case BasicType::double__complex_:
            break;
        case BasicType::long_double_complex_:
            break;
        case BasicType::float__imaginary_:
            break;
        case BasicType::double__imaginary_:
            break;
        case BasicType::long_double_imaginary_:
            break;
        case BasicType::float_:
            break;
        case BasicType::double_:
            break;
        case BasicType::long_double_:
            break;
        case BasicType::unsigned_:
        case BasicType::int_:
            break;
        case BasicType::char8_t_:
            break;
        case BasicType::char16_t_:
            break;
        case BasicType::char32_t_:
            break;
        case BasicType::unsigned_long_long_:
        case BasicType::long_long_:
            break;
        case BasicType::unsigned_long_:
        case BasicType::long_:
            break;
        case BasicType::bitint_:
        case BasicType::unsigned_bitint_:
            break;
        case BasicType::wchar_t_:
            break;
        case BasicType::unsigned_short_:
        case BasicType::short_:
            break;
        case BasicType::unsigned_char_:
        case BasicType::signed_char_:
        case BasicType::char_:
            break;
        case BasicType::inative_:
        case BasicType::unative_:
            break;
        case BasicType::bool_:
            break;
        case BasicType::bit_:
            break;
        case BasicType::void_:
            break;
        case BasicType::pointer_:
        case BasicType::memberptr_:
        case BasicType::const_:
        case BasicType::volatile_:
        case BasicType::lref_:
        case BasicType::rref_:
        case BasicType::lrqual_:
        case BasicType::rrqual_:
        case BasicType::derivedfromtemplate_:
            return hasPackedTemplate(tp->btp);
        case BasicType::seg_:
            break;
        case BasicType::ellipse_:
            break;
        case BasicType::any_:
            break;
        case BasicType::string_:
            break;
        case BasicType::object_:
            break;
        case BasicType::class_:
            break;
        case BasicType::struct_:
            break;
        case BasicType::union_:
            break;
        case BasicType::enum_:
            break;
        case BasicType::templateparam_:
            return tp->templateParam->second->packed;
        default:
            diag("hasPackedTemplateParam: unknown type");
            break;
    }
    return 0;
}
void checkPackedType(SYMBOL* sym)
{
    if (sym->sb->storage_class != StorageClass::parameter_)
    {
        error(ERR_PACK_SPECIFIER_MUST_BE_USED_IN_PARAMETER);
    }
}
std::stack<EXPRESSION*> iterateToPostOrder(EXPRESSION* exp)
{
    std::stack<EXPRESSION*> st1;
    st1.push(exp);
    std::stack<EXPRESSION*> st2;
    while (!st1.empty())
    {
        EXPRESSION* curr = st1.top();
        st1.pop();
        st2.push(curr);
        if (curr->left)
        {
            st1.push(curr->left);
        }
        if (curr->right)
        {
            st1.push(curr->right);
        }
    }
    return st2;
}
bool hasPackedExpression(EXPRESSION* exp, bool useAuto)
{
    if (!exp)
        return false;
    std::stack<EXPRESSION*> st = iterateToPostOrder(exp);
    while (!st.empty())
    {
        EXPRESSION* exp1 = st.top();
        st.pop();
        if (useAuto && exp1->type == ExpressionNode::auto_)
        {
            if (exp1->v.sp->packed)
            {
                return true;
            }
            else
            {
                continue;
            }
        }
        if (exp1->type == ExpressionNode::global_ || exp1->type == ExpressionNode::const_)
        {
            if (exp1->v.sp->templateParams)
                for (auto&&tpl : *exp1->v.sp->templateParams)
                    if (tpl.second->packed)
                        return true;
        }
        if (exp1->type == ExpressionNode::callsite_)
        {
            if (useAuto)
            {
                if (exp1->v.func->templateParams)
                    for (auto&& tpl : *exp1->v.func->templateParams)
                        if (tpl.second->packed)
                            return true;
            }
            if (exp1->v.func->arguments)
                for (auto il : *exp1->v.func->arguments)
                    if (hasPackedExpression(il->exp, useAuto))
                        return true;
            if (exp1->v.func->thisptr && hasPackedExpression(exp1->v.func->thisptr, useAuto))
                return true;
        }
        if (exp1->type == ExpressionNode::templateparam_)
        {
            if (exp1->v.sp->tp->BaseType()->templateParam->second->packed)
                return true;
        }
        if (exp1->type == ExpressionNode::templateselector_)
        {
            if (useAuto)
            {
                for (auto&& i : *exp1->v.templateSelector)
                {
                    if (i.isTemplate && i.templateParams)
                    {
                        for (auto&& tpl : *i.templateParams)
                            if (tpl.second->packed)
                                return true;
                    }
                }
            }
        }
    }
    return false;
}
void checkPackedExpression(EXPRESSION* exp)
{
    if (!hasPackedExpression(exp, true) && !definingTemplate)
        error(ERR_PACK_SPECIFIER_REQUIRES_PACKED_FUNCTION_PARAMETER);
}
void checkUnpackedExpression(EXPRESSION* exp)
{
    if (hasPackedExpression(exp, false))
        error(ERR_PACK_SPECIFIER_REQUIRED_HERE);
}
void GatherPackedVars(int* count, SYMBOL** arg, EXPRESSION* packedExp);
void GatherPackedTypes(int* count, SYMBOL** arg, Type* tp);
void GatherTemplateParams(int* count, SYMBOL** arg, std::list<TEMPLATEPARAMPAIR>* tplx)
{
    if (tplx)
    {
        for (auto&& tpl : *tplx)
        {
            if (tpl.second->packed && tpl.first && tpl.second->type == TplType::typename_)
            {
                arg[(*count)++] = /*sym*/ tpl.first;
                NormalizePacked(tpl.first->tp);
            }
            else if (tpl.second->type == TplType::int_)
            {
                if (tpl.second->byNonType.dflt)
                {
                    if (tpl.second->packed)
                    {
                        if (tpl.first)
                            arg[(*count)++] = tpl.first;
                        if (tpl.second->byPack.pack)
                            for (auto&& tpl1 : *tpl.second->byPack.pack)
                                GatherPackedVars(count, arg, tpl1.second->byNonType.dflt);
                    }
                    else
                    {
                        GatherPackedVars(count, arg, tpl.second->byNonType.dflt);
                    }
                }
            }
            else if (tpl.second->type == TplType::typename_)
            {
                if (tpl.second->byClass.dflt)
                {
                    if (tpl.second->packed)
                    {
                        if (tpl.second->byPack.pack)
                            for (auto&& tpl1 : *tpl.second->byPack.pack)
                                GatherPackedTypes(count, arg, tpl1.second->byClass.dflt);
                    }
                    else if (tpl.second->byClass.dflt->IsDeferred())
                    {
                        GatherTemplateParams(count, arg, tpl.second->byClass.dflt->BaseType()->templateArgs);
                    }
                    else
                    {
                        GatherPackedTypes(count, arg, tpl.second->byClass.dflt);
                    }
                }
            }
        }
    }
}
void GatherPackedTypes(int* count, SYMBOL** arg, Type* tp)
{
    if (tp)
    {
        if (tp->type == BasicType::typedef_ && tp->sp->templateParams)
        {
            GatherTemplateParams(count, arg, tp->sp->templateParams);
        }
        else if (tp->BaseType()->type == BasicType::templateselector_)
        {
            for (auto&& tsl : *tp->BaseType()->sp->sb->templateSelector)
            {
                if (tsl.templateParams)
                {
                    GatherTemplateParams(count, arg, tsl.templateParams);
                }
            }
        }
        else if (tp->IsStructured())
        {
            if (tp->BaseType()->sp->sb->templateLevel)
            {
                GatherTemplateParams(count, arg, tp->BaseType()->sp->templateParams);
            }
        }
        else if (tp->IsDeferred())
        {
            GatherTemplateParams(count, arg, tp->BaseType()->templateArgs);
        }
    }
}
void GatherPackedVars(int* count, SYMBOL** arg, EXPRESSION* packedExp)
{
    if (!packedExp)
        return;
    GatherPackedVars(count, arg, packedExp->left);
    GatherPackedVars(count, arg, packedExp->right);
    if (packedExp->type == ExpressionNode::auto_ && packedExp->v.sp->packed)
    {
        arg[(*count)++] = packedExp->v.sp;
        NormalizePacked(packedExp->v.sp->tp);
    }
    else if (packedExp->type == ExpressionNode::global_ && packedExp->v.sp->sb->parentClass)
    {
        // undefined
        SYMBOL* spx = packedExp->v.sp->sb->parentClass;
        while (spx)
        {
            GatherTemplateParams(count, arg, spx->templateParams);
            spx = spx->sb->parentClass;
        }
    }
    else if (packedExp->type == ExpressionNode::callsite_)
    {
        GatherTemplateParams(count, arg, packedExp->v.func->templateParams);
        Argument* lst;
        if (packedExp->v.func->arguments)
            for (auto lst : *packedExp->v.func->arguments)
                GatherPackedVars(count, arg, lst->exp);
    }
    else if (packedExp->type == ExpressionNode::templateselector_)
    {
        for (auto&& tsl : *packedExp->v.templateSelector)
        {
            if (tsl.templateParams)
            {
                GatherTemplateParams(count, arg, tsl.templateParams);
            }
        }
    }
    else if (packedExp->type == ExpressionNode::templateparam_)
    {
        arg[(*count)++] = packedExp->v.sp;
        NormalizePacked(packedExp->v.sp->tp);
    }
    else if (packedExp->type == ExpressionNode::construct_)
    {
        GatherPackedTypes(count, arg, packedExp->v.construct.tp);
    }
    else if (packedExp->type == ExpressionNode::global_ || packedExp->type == ExpressionNode::const_)
    {
        if (packedExp->v.sp->templateParams)
        {
            GatherTemplateParams(count, arg, packedExp->v.sp->templateParams);
        }

    }
}
EXPRESSION* ReplicatePackedVars(int count, SYMBOL** arg, EXPRESSION* packedExp, int index);
    Type* ReplicatePackedTypes(int count, SYMBOL** arg, Type* tp, int index);
std::list<TEMPLATEPARAMPAIR>* ReplicateTemplateParams(int count, SYMBOL** arg, std::list<TEMPLATEPARAMPAIR>* tplx, int index)
{
    if (tplx)
    {
        std::list<TEMPLATEPARAMPAIR>* rv = templateParamPairListFactory.CreateList();
        for (auto&& tpl : *tplx)
        {
            rv->push_back(TEMPLATEPARAMPAIR{ nullptr, nullptr });
            if (tpl.second->packed && tpl.first)
            {
                auto tparam = Allocate<TEMPLATEPARAM>();
                rv->back().second = tparam;
                tparam->packed = false;
                for (int j = 0; j < count; j++)
                    if (!strcmp(arg[j]->name, tpl.first->name))
                    {
                        auto ittpl1 = arg[j]->tp->templateParam->second->byPack.pack->begin();
                        auto ittpl1e = arg[j]->tp->templateParam->second->byPack.pack->end();
                        for (int k = 0; k < index && ittpl1 != ittpl1e; k++, ++ittpl1);
                        tparam->byClass.val = ittpl1->second->byClass.val ? ittpl1->second->byClass.val : ittpl1->second->byClass.dflt;
                        break;
                    }
            }
            else if (tpl.second->type == TplType::int_)
            {
                if (tpl.second->byNonType.dflt)
                {
                    auto tparam = Allocate<TEMPLATEPARAM>();
                    rv->back().second = tparam;
                    *tparam = *tpl.second;
                    tparam->packed = false;
                    tparam->byNonType.val = ReplicatePackedVars(count, arg, tpl.second->byNonType.dflt, index);
                }
            }
            else if (tpl.second->type == TplType::typename_)
            {
                if (tpl.second->byClass.dflt)
                {
                    auto tparam = Allocate<TEMPLATEPARAM>();
                    rv->back().second = tparam;
                    *tparam = *tpl.second;
                    tparam->packed = false;
                    tparam->byNonType.val = ReplicatePackedVars(count, arg, tpl.second->byNonType.dflt, index);
                }
            }
        }
        return rv;
    }
    return nullptr;
}
Type* ReplicatePackedTypes(int count, SYMBOL** arg, Type* tp, int index)
{
    if (tp)
    {
        if (tp->type == BasicType::templateselector_)
        {
            tp = tp->CopyType();
            auto old = tp->sp->sb->templateSelector;
            auto tsl = tp->sp->sb->templateSelector = templateSelectorListFactory.CreateVector();
            for (auto&& x : *old)
            {
                tsl->push_back(x);
                if (x.templateParams)
                {
                    tsl->back().templateParams = ReplicateTemplateParams(count, arg, x.templateParams, index);
                }
            }
        }
        else if (tp->IsStructured())
        {
            if (tp->BaseType()->sp->sb->templateLevel)
            {
                tp = tp->CopyType(true, [count, arg, index](Type*& old, Type*& newx) {
                    if (old->rootType == old)
                        newx->sp->templateParams = ReplicateTemplateParams(count, arg, newx->sp->templateParams, index);
                });
            }
        }
    }
    return tp;
}
EXPRESSION* ReplicatePackedVars(int count, SYMBOL** arg, EXPRESSION* packedExp, int index)
{
    if (!packedExp)
        return packedExp;
    packedExp->left = ReplicatePackedVars(count, arg, packedExp->left, index);
    packedExp->right = ReplicatePackedVars(count, arg, packedExp->right, index);

    if (packedExp->type == ExpressionNode::auto_ && packedExp->v.sp->packed)
    {
        for (int j = 0; j < count; j++)
            if (!strcmp(arg[j]->name, packedExp->v.sp->name))
            {
                auto tpl = arg[j]->templateParams->front().second->byPack.pack->begin();
                auto tple = arg[j]->templateParams->front().second->byPack.pack->end();
                for (int k = 0; k < index && tpl != tple; k++, ++tpl);
                packedExp = tpl->second->byNonType.dflt;
                break;
            }
    }
    else
    {
        EXPRESSION* rv = Allocate<EXPRESSION>();
        *rv = *packedExp;
        packedExp = rv;
        if (packedExp->type == ExpressionNode::global_ && packedExp->v.sp->sb->parentClass)
        {
            // undefined
            SYMBOL** spx = &packedExp->v.sp;

            while (*spx)
            {
                auto next = Allocate<SYMBOL>();
                *next = **spx;
                if (next->templateParams)
                    next->templateParams = ReplicateTemplateParams(count, arg, next->templateParams, index);
                if ((*spx)->sb->parentClass)
                {
                    next->sb = Allocate<sym::_symbody>();
                    *next->sb = *(*spx)->sb;
                }
                spx = &(*spx)->sb->parentClass;
            }
        }
        else if (packedExp->type == ExpressionNode::callsite_)
        {
            packedExp->v.func->templateParams = ReplicateTemplateParams(count, arg, packedExp->v.func->templateParams, index);
            if (packedExp->v.func->arguments)
            {
                std::list<Argument*> temp;
                for (auto old : *packedExp->v.func->arguments)
                {
                    auto lst = Allocate<Argument>();
                    *lst = *old;
                    lst->exp = ReplicatePackedVars(count, arg, lst->exp, index);
                    temp.push_back(lst);
                }
                *packedExp->v.func->arguments = temp;
            }
        }
        else if (packedExp->type == ExpressionNode::templateselector_)
        {
            auto old = packedExp->v.templateSelector;
            auto tsl = packedExp->v.templateSelector = templateSelectorListFactory.CreateVector();
            for (auto&& x : *old)
            {
                tsl->push_back(x);
                if (x.templateParams)
                {
                    tsl->back().templateParams = ReplicateTemplateParams(count, arg, x.templateParams, index);
                }
            }
        }
        else if (packedExp->type == ExpressionNode::construct_)
        {
            packedExp->v.construct.tp = ReplicatePackedTypes(count, arg, packedExp->v.construct.tp, index);
        }
    }
    return packedExp;
}
int CountPacks(std::list<TEMPLATEPARAMPAIR>* packs)
{
    int rv = 0;
    if (packs)
    {
        return packs->size();
    }
    return rv;
}
void expandPackedInitList(std::list<Argument*>** lptr, SYMBOL* funcsp, LexList* start, EXPRESSION* packedExp)
{
    if (packedExp->type == ExpressionNode::templateparam_)
    {
        if (packedExp->v.sp->tp->templateParam->second->packed)
        {
            if (packedExp->v.sp->tp->templateParam->second->byPack.pack)
            {
                if (!*lptr)
                    *lptr = argumentListFactory.CreateList();
                for (auto&& t : *packedExp->v.sp->tp->templateParam->second->byPack.pack)
                {
                    auto il = Allocate<Argument>();
                    il->exp = t.second->byNonType.val;
                    il->tp = t.second->byNonType.tp;
                    (*lptr)->push_back(il);
                }
            }
        }
    }
    else
    {
        int oldPack = packIndex;
        int count = 0;
        SYMBOL* arg[200];
        GatherPackedVars(&count, arg, packedExp);
        expandingParams++;
        if (count)
        {
            if (arg[0]->sb && arg[0]->packed && arg[0]->sb->parent)
            {
                if (!*lptr)
                    *lptr = argumentListFactory.CreateList();
                auto it = arg[0]->sb->parent->tp->BaseType()->syms->begin();
                auto itend = arg[0]->sb->parent->tp->BaseType()->syms->end();
                for (; it != itend && (*it) != arg[0]; ++it);
                if (it != itend)
                {
                    while (it != itend)
                    {
                        SYMBOL* sym = *it;
                        Argument* p = Allocate<Argument>();
                        p->tp = sym->tp;
                        p->exp = MakeExpression(ExpressionNode::auto_, sym);
                        if (p->tp->IsRef())
                        {
                            p->exp = MakeExpression(ExpressionNode::l_ref_, p->exp);
                            p->tp = p->tp->BaseType()->btp;
                        }
                        if (!p->tp->IsStructured())
                            Dereference(p->tp, &p->exp);
                        (*lptr)->push_back(p);
                        ++it;
                    }
                }
            }
            else
            {
                int i;
                int n = CountPacks(arg[0]->tp->templateParam->second->byPack.pack);
                if (n > 1 || !packedExp->v.func->arguments || !packedExp->v.func->arguments->size() || packedExp->v.func->arguments->front()->tp->type != BasicType::void_)
                {
                    if (!*lptr)
                        *lptr = argumentListFactory.CreateList();

                    for (i = 0; i < n; i++)
                    {
                        Argument* p = Allocate<Argument>();
                        LexList* lex = SetAlternateLex(start);
                        packIndex = i;
                        expression_assign(lex, funcsp, nullptr, &p->tp, &p->exp, nullptr, _F_PACKABLE);
                        optimize_for_constants(&p->exp);
                        SetAlternateLex(nullptr);
                        if (p->tp->type != BasicType::void_)
                            if (p->tp)
                            {
                                (*lptr)->push_back(p);
                            }
                    }
                }
            }
        }
        expandingParams--;
        packIndex = oldPack;
    }
}
static int GetBaseClassList(const char* name, SYMBOL* cls, std::list<BASECLASS*>* bases, std::list<BASECLASS*>& result)
{
    (void)cls;
    int n = 0;
    char str[1024];
    char* clslst[100];
    char *p = str, *c;
    int ccount = 0;
    strcpy(str, name);
    while ((c = (char*)strstr(p, "::")))
    {
        clslst[n++] = p;
        p = c;
        *p = 0;
        p += 2;
    }
    clslst[n++] = p;

    if (bases)
        for (auto bc : *bases)
            if (!bc->isvirtual)
            {
                SYMBOL* parent = bc->cls;
                int i;
                if (parent->tp->type == BasicType::typedef_)
                    parent = parent->tp->BaseType()->sp;
                for (i = n - 1; i >= 0 && parent;
                     i--, parent = parent->sb->parentClass ? parent->sb->parentClass : parent->sb->parentNameSpace)
                    if (strcmp(parent->name, clslst[i]))
                        break;
                if (i < 0 || (i == 0 && parent == nullptr && clslst[0][0] == '\0'))
                {
                    ++ccount;
                    result.push_back(bc);
                }
            }
    return ccount;
}
static int GetVBaseClassList(const char* name, SYMBOL* cls, std::list<VBASEENTRY*>* vbases, std::list<VBASEENTRY*>& result)
{
    (void)cls;
    int n = 0;
    char str[1024];
    char* clslst[100];
    char *p = str, *c;
    int vcount = 0;
    strcpy(str, name);
    while ((c = (char*)strstr(p, "::")))
    {
        clslst[n++] = p;
        p = c;
        *p = 0;
        p += 2;
    }
    clslst[n++] = p;

    if (vbases)
    {
        for (auto vbase : *vbases)
        {
            SYMBOL* parent = vbase->cls;
            int i;
            if (parent->tp->type == BasicType::typedef_)
                parent = parent->tp->BaseType()->sp;
            for (i = n - 1; i >= 0 && parent;
                i--, parent = parent->sb->parentClass ? parent->sb->parentClass : parent->sb->parentNameSpace)
                if (strcmp(parent->name, clslst[i]))
                    break;
            if (i < 0 || (i == 0 && parent == nullptr && clslst[0][0] == '\0'))
            {
                ++vcount;
                result.push_back(vbase);
            }
        }
    }
    return vcount;
}
void expandPackedBaseClasses(SYMBOL* cls, SYMBOL* funcsp, std::list<MEMBERINITIALIZERS*>::iterator& init,
                             std::list<MEMBERINITIALIZERS*>::iterator& initend,
                             std::list<MEMBERINITIALIZERS*>* mi, std::list<BASECLASS*>* bc,
                                            std::list<VBASEENTRY*>* vbase)
{
    MEMBERINITIALIZERS* linit = *init;
    int basecount = 0, vbasecount = 0;
    std::list<BASECLASS*> baseEntries;
    std::list<VBASEENTRY*> vbaseEntries;
    basecount = GetBaseClassList(linit->name, cls, bc, baseEntries);
    if (!basecount)
        vbasecount = GetVBaseClassList(linit->name, cls, vbase, vbaseEntries);
    if (!basecount && !vbasecount)
    {
        // already evaluated to not having a base class
        ++init;
    }
    else if ((basecount && !baseEntries.front()->cls->sb->templateLevel) || (vbasecount && !vbaseEntries.front()->cls->sb->templateLevel))
    {
        ++init;
        errorsym(ERR_NOT_A_TEMPLATE, linit->sp);
    }
    else
    {
        LexList* lex = SetAlternateLex(linit->initData);
        init = mi->erase(init);
        if (MATCHKW(lex, Keyword::lt_))
        {
            // at this point we've already created the independent base classes
            // but the initdata has the argument list, so get it out of the way
            // and also count the number of packs to see if it matches the number of templates..
            int n = -1;
            std::list<TEMPLATEPARAMPAIR>* lst = nullptr;
            LexList* arglex = GetTemplateArguments(lex, funcsp, linit->sp, &lst);
            std::deque<Type*> defaults;
            PushPopDefaults(defaults, funcsp->templateParams, true, true);
            std::stack <TEMPLATEPARAM*> stk; 
            if (funcsp->templateParams)
            {
                for (auto l : *funcsp->templateParams)
                {
                    if (l.second->packed)
                    {
                        if (l.second->byPack.pack)
                            for (auto i : *l.second->byPack.pack)
                                stk.push(i.second);
                    }
                    else if (l.second->type != TplType::new_)
                        stk.push(l.second);
                }
                while (!stk.empty())
                {
                    auto t = stk.top();
                    stk.pop();
                    t->byClass.dflt = t->byClass.val;
                }
            }
            SetAlternateLex(nullptr);
            if (lst)
            {
                for (auto&& pack : *lst)
                {
                    if (pack.second->packed)
                    {
                        int n1 = CountPacks(pack.second->byPack.pack);
                        if (n == -1)
                        {
                            n = n1;
                        }
                        /*

                        else if (n != n1)
                        {
                            error(ERR_PACK_SPECIFIERS_SIZE_MISMATCH);
                            break;
                        }
                        */
                    }
                }
            }
            if (n > 0)
            {
                int i;
                // the presumption is that the number of packed vars will be the same as the number
                // of instantiated templates...
                //
                // I have also seen n be 0 during parsing...
                auto itb = baseEntries.begin();
                auto itv = vbaseEntries.begin();
                int oldArgumentNesting = argumentNesting;
                argumentNesting = -1;
                for (i = 0; i < n; i++)
                {
                    int oldPack = packIndex;
                    SYMBOL* baseSP = basecount ? (*itb)->cls : (*itv)->cls;
                    if (basecount)
                        ++itb;
                    else
                        ++itv;
                    MEMBERINITIALIZERS* added = Allocate<MEMBERINITIALIZERS>();
                    bool done = false;
                    lex = SetAlternateLex(arglex);
                    packIndex = i;
                    added->name = linit->name;
                    init = mi->insert(init, added);
                    ++init;
                    added->sp = baseSP;
                    if (MATCHKW(lex, Keyword::openpa_) && added->sp->tp->sp->sb->trivialCons)
                    {
                        lex = getsym();
                        if (MATCHKW(lex, Keyword::closepa_))
                        {
                            lex = getsym();
                            added->init = nullptr;
                            InsertInitializer(&added->init, nullptr, nullptr, added->sp->sb->offset, false);
                            done = true;
                        }
                        else
                        {
                            lex = backupsym();
                        }
                    }
                    if (!done)
                    {
                        SYMBOL* sym = makeID(StorageClass::member_, added->sp->tp, nullptr, added->sp->name);
                        CallSite shim;
                        added->sp = sym;
                        shim.arguments = nullptr;
                        getMemberInitializers(lex, funcsp, &shim, MATCHKW(lex, Keyword::openpa_) ? Keyword::closepa_ : Keyword::end_, false);
                        if (shim.arguments)
                        {
                            added->init = initListFactory.CreateList();
                            for (auto a : *shim.arguments)
                            {
                                auto xinit = Allocate<Initializer>();
                                xinit->basetp = a->tp;
                                xinit->exp = a->exp;
                                added->init->push_back(xinit);
                            }
                        }
                    }
                    SetAlternateLex(nullptr);
                    packIndex = oldPack;
                }
                argumentNesting = oldArgumentNesting;
            }
            PushPopDefaults(defaults, funcsp->templateParams, true, false);
        }
        else
        {
            lex = SetAlternateLex(nullptr);
            SpecializationError((char*)nullptr);
        }
    }
}
void ExpandExpression(LexList* start, SYMBOL* funcsp, EXPRESSION** exp)
{
    // this is going to presume that the expression involved
    // is not too long to be cached by the LexList mechanism.
    int oldPack = packIndex;
    int count = 0;
    SYMBOL* arg[200];
    GatherPackedVars(&count, arg, *exp);
    expandingParams++;
    if (count)
    {
        *exp = nullptr;
        EXPRESSION** next = exp;
        int i;
        int n = CountPacks(arg[0]->tp->templateParam->second->byPack.pack);
        for (i = 0; i < n; i++)
        {
            Argument* p = Allocate<Argument>();
            LexList* lex = SetAlternateLex(start);
            Type* tp1;
            EXPRESSION* exp1;
            packIndex = i;
            expression_comma(lex, funcsp, nullptr, &tp1, &exp1, nullptr, _F_PACKABLE);
            SetAlternateLex(nullptr);
            if (tp1)
            {
                if (!*next)
                {
                    *next = exp1;
                }
                else
                {
                    *next = MakeExpression(ExpressionNode::comma_, *next, exp1);
                    next = &(*next)->right;
                }
            }
        }
    }
    expandingParams--;
    packIndex = oldPack;
}
void expandPackedMemberInitializers(SYMBOL* cls, SYMBOL* funcsp, std::list<TEMPLATEPARAMPAIR>* templatePack, std::list<MEMBERINITIALIZERS*>** p,
                                    LexList* start, std::list<Argument*>* list)
{
    int n = CountPacks(templatePack);
    MEMBERINITIALIZERS* orig = (*p)->front();
    (*p)->pop_front();
    auto itp = (*p)->begin();
    if (n)
    {
        int count = 0;
        int i;
        int oldPack;
        SYMBOL* arg[300];
        if (templatePack)
        {
            for (auto&& pack : *templatePack)
            {
                if (!pack.second->byClass.val->IsStructured())
                {
                    error(ERR_STRUCTURED_TYPE_EXPECTED_IN_PACKED_TEMPLATE_PARAMETER);
                    return;
                }
            }
        }
        if (list)
            for (auto init : *list)
                GatherPackedVars(&count, arg, init->exp);
        for (i = 0; i < count; i++)
        {
            if (CountPacks(arg[i]->tp->templateParam->second->byPack.pack) != n)
            {
                error(ERR_PACK_SPECIFIERS_SIZE_MISMATCH);
                break;
            }
        }
        oldPack = packIndex;
        auto ittpp = templatePack->begin();
        auto ittppe = templatePack->end();
        for (i = 0; i < n; i++)
        {
            LexList* lex = SetAlternateLex(start);
            MEMBERINITIALIZERS* mi = Allocate<MEMBERINITIALIZERS>();
            Type* tp = ittpp->second->byClass.val;
            int offset = 0;
            int vcount = 0, ccount = 0;
            *mi = *orig;
            mi->sp = nullptr;
            packIndex = i;
            mi->name = ittpp->second->byClass.val->sp->name;
            if (cls->sb->baseClasses)
                for (auto bc : *cls->sb->baseClasses)
                    if (!strcmp(bc->cls->name, mi->name))
                    {
                        if (bc->isvirtual)
                            vcount++;
                        else
                            ccount++;
                        mi->sp = bc->cls;
                        offset = bc->offset;
                    }
            if ((ccount && vcount) || ccount > 1)
            {
                errorsym2(ERR_NOT_UNAMBIGUOUS_BASE, mi->sp, cls);
            }
            if (mi->sp && mi->sp == tp->sp)
            {
                bool done = false;
                mi->sp->sb->offset = offset;
                if (MATCHKW(lex, Keyword::openpa_) && mi->sp->tp->sp->sb->trivialCons)
                {
                    lex = getsym();
                    if (MATCHKW(lex, Keyword::closepa_))
                    {
                        lex = getsym();
                        mi->init = nullptr;
                        InsertInitializer(&mi->init, nullptr, nullptr, mi->sp->sb->offset, false);
                        done = true;
                    }
                    else
                    {
                        lex = backupsym();
                    }
                }
                if (!done)
                {
                    SYMBOL* sym = makeID(StorageClass::member_, mi->sp->tp, nullptr, mi->sp->name);
                    CallSite shim;
                    mi->sp = sym;
                    lex = SetAlternateLex(mi->initData);
                    shim.arguments = nullptr;
                    getMemberInitializers(lex, funcsp, &shim, MATCHKW(lex, Keyword::openpa_) ? Keyword::closepa_ : Keyword::end_, false);
                    SetAlternateLex(nullptr);
                    if (shim.arguments)
                    {
                        mi->init = initListFactory.CreateList();
                        for (auto a : *shim.arguments)
                        {
                            auto xinit = Allocate<Initializer>();
                            xinit->basetp = a->tp;
                            xinit->exp = a->exp;
                            mi->init->push_back(xinit);
                        }
                    }
                }
            }
            else
            {
                mi->sp = nullptr;
            }
            if (mi->sp)
            {
                (*p)->insert(itp, mi);
                ++itp;
            }
            else
            {
                errorstrsym(ERR_NOT_A_MEMBER_OR_BASE_CLASS, mi->name, cls);
            }
            SetAlternateLex(nullptr);
        }
        packIndex = oldPack;
    }
}

std::list<Argument*>* ExpandTemplateArguments(EXPRESSION* exp)
{
    std::list<Argument*>* rv = nullptr;
    bool dofunc = false;
    bool doparam = false;
    if (exp->v.func->arguments)
    {
        for (auto arg : *exp->v.func->arguments)
        {
            if (arg->exp && (arg->exp->type == ExpressionNode::callsite_ || arg->exp->type == ExpressionNode::funcret_))
            {
                dofunc = true;
            }
            if (arg->tp && arg->tp->BaseType()->type == BasicType::templateparam_)
            {
                doparam |= !definingTemplate || instantiatingTemplate;
            }
        }
        if (doparam)
        {
            Type* tp = nullptr;
            for (auto arg : *exp->v.func->arguments)
            {
                Type* tp1 = arg->tp->BaseType();
                if (tp1 && tp1->type == BasicType::templateparam_)
                {
                    if (tp1->templateParam->second->packed)
                    {
                        if (tp1->templateParam->second->byPack.pack)
                        {
                            for (auto&& tpx : *tp1->templateParam->second->byPack.pack)
                            {
                                auto dflt = tpx.second->byClass.val;
                                if (!dflt)
                                    dflt = tpx.second->byClass.dflt;
                                if (dflt)
                                {
                                    tp = tpx.second->byClass.val;
                                    if (arg->tp->IsConst())
                                        tp = Type::MakeType(BasicType::const_, tp);
                                    if (arg->tp->IsVolatile())
                                        tp = Type::MakeType(BasicType::volatile_, tp);
                                    if (!rv)
                                        rv = argumentListFactory.CreateList();
                                    auto arg1 = Allocate<Argument>();
                                    arg1->tp = tp;
                                    arg1->exp = MakeIntExpression(ExpressionNode::c_i_, 0);
                                    rv->push_back(arg1);
                                }
                            }
                        }
                    }
                    else
                    {
                        auto arg1 = Allocate<Argument>();
                        *arg1 = *arg;
                        tp = tp1->templateParam->second->byClass.val;
                        if (tp)
                        {
                            if (arg->tp->IsConst())
                                tp = Type::MakeType(BasicType::const_, tp);
                            if (arg->tp->IsVolatile())
                                tp = Type::MakeType(BasicType::volatile_, tp);
                            arg1->tp = tp;
                        }
                        if (!rv)
                            rv = argumentListFactory.CreateList();
                        rv->push_back(arg1);
                    }
                }
                else
                {
                    auto arg1 = Allocate<Argument>();
                    *arg1 = *arg;
                    if (!rv)
                        rv = argumentListFactory.CreateList();
                    rv->push_back(arg1);
                }
            }
        }
        if (dofunc)
        {
            for (auto arg : *exp->v.func->arguments)
            {
                if (arg->exp)
                {
                    SYMBOL* syms[200];
                    int count = 0, n = 0;
                    GatherPackedVars(&count, syms, arg->exp);
                    if (count)
                    {
                        for (int i = 0; i < count; i++)
                        {
                            int n1 = CountPacks(syms[i]->tp->templateParam->second->byPack.pack);
                            if (n1 > n)
                                n = n1;
                        }
                        int oldIndex = packIndex;
                        for (int i = 0; i < n; i++)
                        {
                            std::deque<TEMPLATEPARAM*> defaults;
                            std::deque<std::pair<Type**, Type*>> types;
                            packIndex = i;
                            auto arg1 = Allocate<Argument>();
                            *arg1 = *arg;
                            if (!rv)
                                rv = argumentListFactory.CreateList();
                            rv->push_back(arg1);
                            if (arg1->exp->type == ExpressionNode::callsite_)
                            {
                                if (arg1->exp->v.func->templateParams)
                                {
                                    for (auto&& tpx : *arg1->exp->v.func->templateParams)
                                    {
                                        if (tpx.second->type != TplType::new_)
                                        {
                                            defaults.push_back(tpx.second);
                                            if (tpx.second->packed && tpx.second->byPack.pack)
                                            {
                                                auto it = tpx.second->byPack.pack->begin();
                                                auto ite = tpx.second->byPack.pack->end();
                                                for (int j = 0; j < packIndex && it != ite; ++j, ++it)
                                                    ;
                                                if (it != ite)
                                                    tpx.second = it->second;
                                            }
                                        }
                                    }
                                }
                                if (arg1->exp->v.func->arguments)
                                {
                                    for (auto il : *arg1->exp->v.func->arguments)
                                    {
                                        Type** tp = &il->tp;
                                        while ((*tp)->btp)
                                            tp = &(*tp)->btp;
                                        if ((*tp)->type == BasicType::templateparam_)
                                        {
                                            auto tpx = (*tp)->templateParam;
                                            if (tpx->second->packed && tpx->second->byPack.pack)
                                            {
                                                auto it = tpx->second->byPack.pack->begin();
                                                auto ite = tpx->second->byPack.pack->end();
                                                for (int j = 0; j < packIndex && it != ite; ++j, ++it)
                                                    ;
                                                if (it != ite && it->second->type == TplType::typename_ && it->second->byClass.val)
                                                {
                                                    types.push_back(std::pair<Type**, Type*>(tp, *tp));
                                                    (*tp) = it->second->byClass.val;
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                            arg1->tp = LookupTypeFromExpression(arg1->exp, nullptr, false);
                            if (arg1->tp && arg1->tp->IsRef())
                            {
                                bool rref = arg1->tp->BaseType()->type == BasicType::rref_;
                                arg1->tp = arg1->tp->BaseType()->btp;
                                if (rref)
                                    (arg1->tp)->rref = true;
                                else
                                    (arg1->tp)->lref = true;
                            }
                            if (arg1->tp == nullptr)
                                arg1->tp = arg->tp;
                            if (arg1->exp->type == ExpressionNode::callsite_)
                            {
                                if (arg1->exp->v.func->templateParams)
                                {
                                    for (auto&& tpx : *arg1->exp->v.func->templateParams)
                                    {
                                        if (tpx.second->type != TplType::new_)
                                        {
                                            tpx.second = defaults.front();
                                            defaults.pop_front();
                                        }
                                    }
                                }
                                for (auto&& t : types)
                                {
                                    *(t.first) = t.second;
                                }
                                types.clear();
                            }
                        }
                        packIndex = oldIndex;
                    }
                    else
                    {
                        auto arg1 = Allocate<Argument>();
                        *arg1 = *arg;
                        arg1->tp = LookupTypeFromExpression(arg1->exp, nullptr, false);
                        if (arg1->tp == nullptr)
                            arg1->tp = &stdany;
                        if (!rv)
                            rv = argumentListFactory.CreateList();
                        rv->push_back(arg1);
                    }
                }
                else
                {
                    auto arg1 = Allocate<Argument>();
                    *arg1 = *arg;
                    if (!rv)
                        rv = argumentListFactory.CreateList();
                    rv->push_back(arg1);
                }
            }
        }
        else if (!rv)
        {
            rv = exp->v.func->arguments;
        }
    }
    return rv;
}
std::list<TEMPLATEPARAMPAIR>** ExpandTemplateArguments(std::list<TEMPLATEPARAMPAIR>** lst, LexList* start, SYMBOL* funcsp, std::list<TEMPLATEPARAMPAIR>* select, bool packable)
{
    int beginning = 0;
    if (*lst)
        beginning = (*lst)->size();
    // this is going to presume that the expression involved
    // is not too long to be cached by the LexList mechanism.
    int oldPack = packIndex;
    int count = 0;
    TEMPLATEPARAMPAIR* arg[500];
    if (!packable)
    {
        if (!*lst)
            *lst = templateParamPairListFactory.CreateList();
        if (select->front().second->packed && packIndex >= 0)
        {
            std::list<TEMPLATEPARAMPAIR>* templateParam = select->front().second->byPack.pack;
            int i;
            auto it = select->front().second->byPack.pack->begin();
            auto ite = select->front().second->byPack.pack->end();
            for (i = 0; i < packIndex && it != ite; i++, ++it);
            if (it != ite)
            {
                (*lst)->push_back(TEMPLATEPARAMPAIR{ nullptr, Allocate<TEMPLATEPARAM>() });
                *(*lst)->back().second = *it->second;
                (*lst)->back().second->ellipsis = false;
                (*lst)->back().second->byClass.dflt = (*lst)->back().second->byClass.val;
                return lst;
            }
        }
        if (select->front().second->ellipsis)
        {
            (*lst)->push_back(TEMPLATEPARAMPAIR{ nullptr, Allocate<TEMPLATEPARAM>() });
            *(*lst)->back().second = *select->front().second;
            (*lst)->back().second->ellipsis = false;
        }
        else
        {
            (*lst)->push_back(TEMPLATEPARAMPAIR(nullptr, select->front().second));
        }
        (*lst)->back().first = select->front().first;
        return lst;
    }
    std::list<TEMPLATEPARAMPAIR> temp = { {select->front().first, select->front().second } };
    GetPackedTypes(arg, &count, &temp);
    expandingParams++;
    if (count)
    {
        int i;
        int n = CountPacks(arg[0]->second->byPack.pack);  // undefined in local context
        for (i = 1; i < count; i++)
        {
            if (CountPacks(arg[i]->second->byPack.pack) != n)
            {
                error(ERR_PACK_SPECIFIERS_SIZE_MISMATCH);
                break;
            }
        }
        for (i = 0; i < n; i++)
        {
            LexList* lex = SetAlternateLex(start);
            Type* tp;
            packIndex = i;
            tp = TypeGenerator::TypeId(lex, funcsp, StorageClass::parameter_, false, true, false);
            SetAlternateLex(nullptr);
            if (tp)
            {
                if (!*lst)
                    *lst = templateParamPairListFactory.CreateList();
                (*lst)->push_back(TEMPLATEPARAMPAIR{ nullptr, Allocate<TEMPLATEPARAM>() });
                (*lst)->back().second->type = TplType::typename_;
                (*lst)->back().second->byClass.dflt = tp;
            }
        }
    }
    else if (select)
    {
        if (!*lst)
            *lst = templateParamPairListFactory.CreateList();
        (*lst)->push_back(select->front());
    }
    expandingParams--;
    packIndex = oldPack;
    // make it packed again...   we aren't flattening at this point.
    if (select->front().second->packed)
    {
        TEMPLATEPARAMPAIR next;
        next.second = Allocate<TEMPLATEPARAM>();
        *next.second = *select->front().second;
        next.first = select->front().first;
        if (*lst && beginning != (*lst)->size())
        {
            next.second->byPack.pack = templateParamPairListFactory.CreateList();
            auto itbeginning = (*lst)->begin();
            int n = (*lst)->size() - beginning;
            while (beginning--)
                ++itbeginning;
            (*next.second->byPack.pack) = std::move(std::list<TEMPLATEPARAMPAIR>(itbeginning, (*lst)->end()));
            while (n--)
                (*lst)->pop_back();
        }
        next.second->resolved = true;
        if (!*lst)
            (*lst) = templateParamPairListFactory.CreateList();
        (*lst)->push_back(next);
    }
    return lst;
}

void ExpandTemplateArguments(std::list<TEMPLATEPARAMPAIR>** lst, LexList* start, SYMBOL* name, SYMBOL* first, SYMBOL* funcsp, Type**tp, EXPRESSION**exp)
{
    // this is going to presume that the expression involved
    // is not too long to be cached by the LexList mechanism.
    int oldPack = packIndex;
    int count = 0;
    SYMBOL* arg[200];
    GatherPackedVars(&count, arg, *exp);
    expandingParams++;
    if (count)
    {
        auto list = templateParamPairListFactory.CreateList();
        int i;
        int n = CountPacks(arg[0]->tp->templateParam->second->byPack.pack);
        for (i = 0; i < n; i++)
        {
            LexList* lex = SetAlternateLex(start);
            packIndex = i;
            expression_assign(lex, funcsp, nullptr, tp, exp, nullptr, _F_PACKABLE);
            if (exp)
            {
                optimize_for_constants(exp);
                while ((*exp)->type == ExpressionNode::comma_ && (*exp)->right)
                    (*exp) = (*exp)->right;
            }
            SetAlternateLex(nullptr);
            if (tp)
            {
                list->push_back(TEMPLATEPARAMPAIR{ name, Allocate<TEMPLATEPARAM>() });
                list->back().second->type = TplType::int_;
                list->back().second->byNonType.dflt = *exp;
                list->back().second->byNonType.tp = *tp;
            }
        }
        if (!*lst)
            *lst = templateParamPairListFactory.CreateList();
        (*lst)->push_back(TEMPLATEPARAMPAIR{ name, Allocate<TEMPLATEPARAM>() });
        (*lst)->back().second->type = TplType::int_;
        (*lst)->back().second->packed = true;
        (*lst)->back().second->byPack.pack = list;
        if (first)
            (*lst)->back().first = first;
    }
    expandingParams--;
    packIndex = oldPack;
}
}  // namespace Parser