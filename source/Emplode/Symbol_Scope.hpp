/**
 *  @note This file is part of Emplode, currently within https://github.com/mercere99/MABE2
 *  @copyright Copyright (C) Michigan State University, MIT Software license; see doc/LICENSE.md
 *  @date 2019-2021.
 *
 *  @file  Symbol_Scope.hpp
 *  @brief Manages a full scope with many internal symbols (including sub-scopes).
 *  @note Status: BETA
 * 
 *  DEVELOPER NOTES:
 *  - Need to fix Add() function to give a user-level error, rather than an assert on duplication.
 */

#ifndef EMPLODE_SYMBOL_SCOPE_HPP
#define EMPLODE_SYMBOL_SCOPE_HPP

#include "emp/base/map.hpp"

#include "Symbol.hpp"
#include "Symbol_Function.hpp"
#include "Symbol_Linked.hpp"
#include "EmplodeTypeBase.hpp"

namespace emplode {

  class EmplodeType;

  // Set of multiple config entries.
  class Symbol_Scope : public Symbol {
  protected:
    using symbol_ptr_t = emp::Ptr<Symbol>;
    using const_symbol_ptr_t = emp::Ptr<const Symbol>;
    emp::map< std::string, symbol_ptr_t > symbol_table;   ///< Map of names to entries.

    ///< If this scope represents a structure, point to it; otherwise set to null.
    emp::Ptr<EmplodeType> obj_ptr = nullptr;
    bool obj_owned = false;

    template <typename T, typename... ARGS>
    T & Add(const std::string & name, ARGS &&... args) {
      auto new_ptr = emp::NewPtr<T>(name, std::forward<ARGS>(args)...);
      emp_assert(!emp::Has(symbol_table, name), "Do not redeclare functions or variables!",
                 name);
      symbol_table[name] = new_ptr;
      return *new_ptr;
    }

    template <typename T, typename... ARGS>
    T & AddBuiltin(const std::string & name, ARGS &&... args) {
      T & result = Add<T, ARGS...>(name, std::forward<ARGS>(args)...);
      result.SetBuiltin();
      return result;
    }

  public:
    Symbol_Scope(const std::string & _name,
                const std::string & _desc,
                emp::Ptr<Symbol_Scope> _scope,
                emp::Ptr<EmplodeType> _obj=nullptr,
                bool _owned=false)
      : Symbol(_name, _desc, _scope), obj_ptr(_obj), obj_owned(_owned) { }

    Symbol_Scope(const Symbol_Scope & in) : Symbol(in) {
      // Copy all defined variables/scopes/functions
      for (auto [name, ptr] : symbol_table) { symbol_table[name] = ptr->Clone(); }
    }
    Symbol_Scope(Symbol_Scope &&) = default;

    ~Symbol_Scope() {
      // If this scope owns its object pointer, delete it now.
      if (obj_owned) obj_ptr.Delete();

      // Clear up the symbol table.
      for (auto [name, ptr] : symbol_table) { ptr.Delete(); }
    }

    emp::Ptr<EmplodeType> GetObjectPtr()  override { return obj_ptr; }  
    emp::Ptr<const EmplodeType> GetObjectPtr() const override { return obj_ptr; }  

    bool IsScope() const override { return true; }
    bool IsLocal() const override { return true; }  // @CAO, for now assuming all scopes are local!

    /// Set this symbol to be a correctly-typed scope pointer.
    emp::Ptr<Symbol_Scope> AsScopePtr() override { return this; }

    /// Get a symbol out of this scope; 
    symbol_ptr_t GetSymbol(std::string name) { return emp::Find(symbol_table, name, nullptr); }

    /// Lookup a variable, scanning outer scopes if needed
    symbol_ptr_t LookupSymbol(const std::string & name, bool scan_scopes=true) override {
      // See if this next symbol is in the var list.
      auto it = symbol_table.find(name);

      // If this name is unknown, check with the parent scope!
      if (it == symbol_table.end()) {
        if (scope.IsNull() || !scan_scopes) return nullptr;  // No parent?  Just fail...
        return scope->LookupSymbol(name);
      }

      // Otherwise we found it!
      return it->second;
    }

    /// Lookup a variable, scanning outer scopes if needed (in const context!)
    const_symbol_ptr_t LookupSymbol(const std::string & name, bool scan_scopes=true) const override {
      // See if this symbol is in the var list.
      auto it = symbol_table.find(name);

      // If this name is unknown, check with the parent scope!
      if (it == symbol_table.end()) {
        if (scope.IsNull() || !scan_scopes) return nullptr;  // No parent?  Just fail...
        return scope->LookupSymbol(name);
      }

      // Otherwise we found it!
      return it->second;
    }

    /// Add a configuration symbol that is linked to a variable - the incoming variable sets
    /// the default and is automatically updated when configs are loaded.
    template <typename VAR_T>
    Symbol_Linked<VAR_T> & LinkVar(const std::string & name,
                                        VAR_T & var,
                                        const std::string & desc,
                                        bool is_builtin = false) {
      if (is_builtin) return AddBuiltin<Symbol_Linked<VAR_T>>(name, var, desc, this);
      return Add<Symbol_Linked<VAR_T>>(name, var, desc, this);
    }

    /// Add a configuration symbol that interacts through a pair of functions - the functions are
    /// automatically called any time the symbol value is accessed (get_fun) or changed (set_fun)
    template <typename VAR_T>
    Symbol_LinkedFunctions<VAR_T> & LinkFuns(const std::string & name,
                                            std::function<VAR_T()> get_fun,
                                            std::function<void(const VAR_T &)> set_fun,
                                            const std::string & desc,
                                            bool is_builtin = false) {
      if (is_builtin) {
        return AddBuiltin<Symbol_LinkedFunctions<VAR_T>>(name, get_fun, set_fun, desc, this);
      }
      return Add<Symbol_LinkedFunctions<VAR_T>>(name, get_fun, set_fun, desc, this);
    }

    /// Add an internal variable of type String.
    Symbol_StringVar & AddStringVar(const std::string & name, const std::string & desc) {
      return Add<Symbol_StringVar>(name, "", desc, this);
    }

    /// Add an internal variable of type Value.
    Symbol_DoubleVar & AddValueVar(const std::string & name, const std::string & desc) {
      return Add<Symbol_DoubleVar>(name, 0.0, desc, this);
    }

    /// Add an internal scope inside of this one.
    Symbol_Scope & AddScope(
      const std::string & name,
      const std::string & desc,
      emp::Ptr<EmplodeType> obj_ptr=nullptr,
      bool obj_owned=false
    ) {
      return Add<Symbol_Scope>(name, desc, this, obj_ptr, obj_owned);
    }

    /// Add a new user-defined function.
    template <typename RETURN_T, typename... ARGS>
    Symbol_Function & AddFunction(const std::string & name,
                              std::function<RETURN_T(ARGS...)> fun,
                              const std::string & desc) {
      return Add<Symbol_Function>(name, fun, desc, this);
    }

    /// Add a new function that is a standard part of the scripting language.
    template <typename RETURN_T, typename... ARGS>
    Symbol_Function & AddBuiltinFunction(const std::string & name,
                                        std::function<RETURN_T(ARGS...)> fun,
                                        const std::string & desc) {
      return AddBuiltin<Symbol_Function>(name, fun, desc, this);
    }

    /// Write out all of the parameters contained in this scope to the provided stream.
    const Symbol & WriteContents(std::ostream & os=std::cout, const std::string & prefix="",
                                      size_t comment_offset=32) const {

      // Loop through all of the entires in this scope and Write them.
      for (auto [name, ptr] : symbol_table) {
        if (ptr->IsBuiltin()) continue; // Skip writing built-in entries.
        ptr->Write(os, prefix, comment_offset);
      }

      return *this;
    }

    /// Write out this scope AND it's contents to the provided stream.
    const Symbol & Write(std::ostream & os=std::cout, const std::string & prefix="",
                              size_t comment_offset=32) const override
    {
      // If this is a built-in scope, don't print it.
      if (IsBuiltin()) return *this;

      // Declare this scope.
      std::string cur_line = prefix;
      if (IsLocal()) cur_line += emp::to_string(GetTypename(), " ");
      cur_line += name;

      bool has_body = emp::AnyOf(symbol_table, [](symbol_ptr_t ptr){ return !ptr->IsBuiltin(); });
      
      // Only open this scope if there are contents.
      cur_line += has_body ? " { " : ";";
      os << cur_line;

      // Indent the comment for the description (if there is one)
      WriteDesc(os, comment_offset, cur_line.size());

      // If we have internal entries, write them out.
      if (has_body) {
        WriteContents(os, prefix+"  ", comment_offset);
        os << prefix << "}\n";      // Close the scope.
      }

      return *this;
    }

    /// Make a copy of this scope and all of the entries inside it.
    symbol_ptr_t Clone() const override { return emp::NewPtr<Symbol_Scope>(*this); }
  };

}
#endif
