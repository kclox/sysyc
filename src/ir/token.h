#ifndef __token_h__
#define __token_h__

#include <variant>
#include <string>

class Token {
  public:
    enum {
        kEnd,
        kGlobal,
        kDeclare,
        kDefine,
        kAlign,
        kTo,
        kZeroinit,

        kConst,

        kI32,
        kI1,
        kVoid,
        kLabelT,

        kCall,
        kRet,
        kBr,
        kIcmp,
        kAdd,
        kSub,
        kMul,
        kSdiv,
        kSrem,
        kAnd,
        kOr,
        kLoad,
        kStore,
        kAlloca,
        kPhi,
        kGetelementptr,
        kZext,
        kBitcast,

        kTmpVar,
        kLocalVar,
        kGlobalVar,
        kTmpLabel,
        kNamedLabel,

        kPtr,
        kEq,
        kX,
        kLbracket,
        kRbracket,
        kLParenthesis,
        kRParenthesis,
        kLBrace,
        kRBrace,
        kComma,

        kInteger,

        kCondEq,
        kCondNe,
        kCondUgt,
        kCondUge,
        kCondUle,
        kCondSgt,
        kCondSge,
        kCondSlt,
        kCondSle,
    };
    int op;
    std::variant<int, std::string> data;

    Token() {}
    Token(int op) : op(op) {}

    operator std::string() const {
        static std::string str_map[] = {
            "kEnd",
            "kGlobal",
            "kDeclare",
            "kDefine",
            "kAlign",
            "kTo",
            "kZeroinit",
            "kConst",
            "kI32",
            "kI1",
            "kVoid",
            "kLabelT",
            "kCall",
            "kRet",
            "kBr",
            "kIcmp",
            "kAdd",
            "kSub",
            "kMul",
            "kSdiv",
            "kSrem",
            "kAnd",
            "kOr",
            "kLoad",
            "kStore",
            "kAlloca",
            "kPhi",
            "kGetelementptr",
            "kZext",
            "kBitcast",
            "kTmpVar",
            "kLocalVar",
            "kGlobalVar",
            "kTmpLabel",
            "kNamedLabel",
            "kPtr",
            "kEq",
            "kX",
            "kLbracket",
            "kRbracket",
            "kLParenthesis",
            "kRParenthesis",
            "kLBrace",
            "kRBrace",
            "kComma",
            "kInteger",
            "kCondEq",
            "kCondNe",
            "kCondUgt",
            "kCondUge",
            "kCondUle",
            "kCondSgt",
            "kCondSge",
            "kCondSlt",
            "kCondSle",
        };
        if (op > kCondSle || op < 0) {
            return "invalid " + std::to_string(op);
        }
        return str_map[op];
    }
    std::string str() const { return this->operator std::string(); }
    bool IsValid() const { return op >= 0 && op <= kCondSle; }
    bool IsEnd() const { return op == kEnd; }
    bool IsBasicType() const { return op >= kI32 && op <= kLabelT; }
    bool IsInst() const { return op >= kCall && op <= kBitcast; }
    bool IsVar() const {
        return op == kLocalVar || op == kGlobalVar || op == kTmpVar;
    }
    bool IsLabel() const { return op == kTmpLabel || op == kNamedLabel; }
    bool IsCond() const { return op >= kCondEq && op <= kCondSle; }
    bool IsBinaryAlu() const { return op >= kAdd && op <= kOr; }
};

#endif