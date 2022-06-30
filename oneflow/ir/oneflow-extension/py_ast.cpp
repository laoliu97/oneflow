#include <set>
#include <sstream>

#include "./py_ast.h"

namespace PythonAST {

typedef std::string identifier;
typedef void* pointer;

mod_t Module(const std::vector<stmt_t>& body) {
    return new Module_(body);
}

stmt_t FunctionDef(const identifier& name, arguments_t args, const
                   std::vector<stmt_t>& body, const std::vector<expr_t>&
                   decorator_list, expr_t returns) {
    return new FunctionDef_(name, args, body, decorator_list, returns);
}

stmt_t Return(expr_t value) {
    return new Return_(value);
}

stmt_t Assign(const std::vector<expr_t>& targets, expr_t value) {
    return new Assign_(targets, value);
}

stmt_t For(expr_t target, expr_t iter, const std::vector<stmt_t>& body, const
           std::vector<stmt_t>& orelse) {
    return new For_(target, iter, body, orelse);
}

stmt_t While(expr_t test, const std::vector<stmt_t>& body, const
             std::vector<stmt_t>& orelse) {
    return new While_(test, body, orelse);
}

stmt_t If(expr_t test, const std::vector<stmt_t>& body, const
          std::vector<stmt_t>& orelse) {
    return new If_(test, body, orelse);
}

stmt_t Raise(expr_t exc, expr_t cause) {
    return new Raise_(exc, cause);
}

stmt_t Assert(expr_t test, expr_t msg) {
    return new Assert_(test, msg);
}

stmt_t Expr(expr_t value) {
    return new Expr_(value);
}

stmt_t Pass() {
    return new Pass_();
}

stmt_t Break() {
    return new Break_();
}

stmt_t Continue() {
    return new Continue_();
}

expr_t BoolOp(boolop_t op, const std::vector<expr_t>& values, int lineno, int
              col_offset) {
    return new BoolOp_(op, values, lineno, col_offset);
}

expr_t BinOp(expr_t left, operator_t op, expr_t right, int lineno, int
             col_offset) {
    return new BinOp_(left, op, right, lineno, col_offset);
}

expr_t Lambda(arguments_t args, expr_t body, int lineno, int col_offset) {
    return new Lambda_(args, body, lineno, col_offset);
}

expr_t IfExp(expr_t test, expr_t body, expr_t orelse, int lineno, int
             col_offset) {
    return new IfExp_(test, body, orelse, lineno, col_offset);
}

expr_t Compare(expr_t left, const std::vector<cmpop_t>& ops, const
               std::vector<expr_t>& comparators, int lineno, int col_offset) {
    return new Compare_(left, ops, comparators, lineno, col_offset);
}

expr_t Call(expr_t func, const std::vector<expr_t>& args, int lineno, int
            col_offset) {
    return new Call_(func, args, lineno, col_offset);
}

expr_t Num(double n, int lineno, int col_offset) {
    return new Num_(n, lineno, col_offset);
}

expr_t Constant(double value, int lineno, int col_offset) {
    return new Constant_(value, lineno, col_offset);
}

expr_t Attribute(expr_t value, const identifier& attr, expr_context_t ctx, int
                 lineno, int col_offset) {
    return new Attribute_(value, attr, ctx, lineno, col_offset);
}

expr_t Name(const identifier& id, expr_context_t ctx, int lineno, int
            col_offset) {
    return new Name_(id, ctx, lineno, col_offset);
}

arguments_t arguments(const std::vector<arg_t>& args, arg_t vararg, const
                      std::vector<arg_t>& kwonlyargs, const
                      std::vector<expr_t>& kw_defaults, arg_t kwarg, const
                      std::vector<expr_t>& defaults) {
    return new arguments_(args, vararg, kwonlyargs, kw_defaults, kwarg,
                          defaults);
}

arg_t arg(const identifier& arg, expr_t annotation, int lineno, int col_offset)
          {
    return new arg_(arg, annotation, lineno, col_offset);
}


class ToStringVisitor : public BaseVisitor {

    std::string to_string(std::string& value) {
        return value;
    }

    template <class T>
    typename std::enable_if<std::is_arithmetic<T>::value, std::string>::type
    to_string(T value) {
        return std::to_string(value);
    }

    template <class T>
    typename std::enable_if<std::is_enum<T>::value, std::string>::type
    to_string(T value) {
        return std::any_cast<std::string>(visit(value));
    }

    template <class T>
    std::string to_string(T* value) {
        std::ostringstream oss;
        oss << value;
        return oss.str();
    }

    template <class T>
    std::string to_string(std::vector<T>& values) {
        std::string result = "[";
        if (!values.empty()) {
            result += to_string(values[0]);
            for (size_t i = 1; i < values.size(); i++) {
                result += ", ";
                result += to_string(values[i]);
            }
        }
        result += "]";
        return result;
    }

    template <class T>
    std::string follow(T value) {
        return std::string();
    }

    std::set<void*> already_visited_;

    template <class T>
    std::string follow(T* value) {
        bool placed = already_visited_.insert(value).second;
        return (placed && value) ? std::any_cast<std::string>(visit(value))
                                 : std::string();
    }

    std::string follow(void* value) {
        return std::string();
    }

    template <class T>
    std::string follow(std::vector<T>& values) {
        std::string result;
        for (auto& value: values) {
            result += follow(value);
        }
        return result;
    }

    size_t depth = 0;
    std::string spaces() {
        std::string result = "";
        for (size_t i = 0; i < depth; i++) {
            result += ' ';
        }
        return result;
    }

public:
    std::any visitModule(Module_t node) override {
        std::string result = spaces() + to_string(node) + " Module(";
        result += "body=" + to_string(node->body);
        result += ")\n";

        depth += 2;
        result += follow(node->body);
        depth -= 2;

        return result;
    }

    std::any visitFunctionDef(FunctionDef_t node) override {
        std::string result = spaces() + to_string(node) + " FunctionDef(";
        result += "name=" + to_string(node->name) + ", " + "args=" +
                  to_string(node->args) + ", " + "body=" +
                  to_string(node->body) + ", " + "decorator_list=" +
                  to_string(node->decorator_list) + ", " + "returns=" +
                  to_string(node->returns);
        result += ")\n";

        depth += 2;
        result += follow(node->name) + follow(node->args) + follow(node->body)
                  + follow(node->decorator_list) + follow(node->returns);
        depth -= 2;

        return result;
    }

    std::any visitReturn(Return_t node) override {
        std::string result = spaces() + to_string(node) + " Return(";
        result += "value=" + to_string(node->value);
        result += ")\n";

        depth += 2;
        result += follow(node->value);
        depth -= 2;

        return result;
    }

    std::any visitAssign(Assign_t node) override {
        std::string result = spaces() + to_string(node) + " Assign(";
        result += "targets=" + to_string(node->targets) + ", " + "value=" +
                  to_string(node->value);
        result += ")\n";

        depth += 2;
        result += follow(node->targets) + follow(node->value);
        depth -= 2;

        return result;
    }

    std::any visitFor(For_t node) override {
        std::string result = spaces() + to_string(node) + " For(";
        result += "target=" + to_string(node->target) + ", " + "iter=" +
                  to_string(node->iter) + ", " + "body=" +
                  to_string(node->body) + ", " + "orelse=" +
                  to_string(node->orelse);
        result += ")\n";

        depth += 2;
        result += follow(node->target) + follow(node->iter) +
                  follow(node->body) + follow(node->orelse);
        depth -= 2;

        return result;
    }

    std::any visitWhile(While_t node) override {
        std::string result = spaces() + to_string(node) + " While(";
        result += "test=" + to_string(node->test) + ", " + "body=" +
                  to_string(node->body) + ", " + "orelse=" +
                  to_string(node->orelse);
        result += ")\n";

        depth += 2;
        result += follow(node->test) + follow(node->body) +
                  follow(node->orelse);
        depth -= 2;

        return result;
    }

    std::any visitIf(If_t node) override {
        std::string result = spaces() + to_string(node) + " If(";
        result += "test=" + to_string(node->test) + ", " + "body=" +
                  to_string(node->body) + ", " + "orelse=" +
                  to_string(node->orelse);
        result += ")\n";

        depth += 2;
        result += follow(node->test) + follow(node->body) +
                  follow(node->orelse);
        depth -= 2;

        return result;
    }

    std::any visitRaise(Raise_t node) override {
        std::string result = spaces() + to_string(node) + " Raise(";
        result += "exc=" + to_string(node->exc) + ", " + "cause=" +
                  to_string(node->cause);
        result += ")\n";

        depth += 2;
        result += follow(node->exc) + follow(node->cause);
        depth -= 2;

        return result;
    }

    std::any visitAssert(Assert_t node) override {
        std::string result = spaces() + to_string(node) + " Assert(";
        result += "test=" + to_string(node->test) + ", " + "msg=" +
                  to_string(node->msg);
        result += ")\n";

        depth += 2;
        result += follow(node->test) + follow(node->msg);
        depth -= 2;

        return result;
    }

    std::any visitExpr(Expr_t node) override {
        std::string result = spaces() + to_string(node) + " Expr(";
        result += "value=" + to_string(node->value);
        result += ")\n";

        depth += 2;
        result += follow(node->value);
        depth -= 2;

        return result;
    }

    std::any visitPass(Pass_t node) override {
        std::string result = spaces() + to_string(node) + " Pass(";
        result += ")\n";

        return result;
    }

    std::any visitBreak(Break_t node) override {
        std::string result = spaces() + to_string(node) + " Break(";
        result += ")\n";

        return result;
    }

    std::any visitContinue(Continue_t node) override {
        std::string result = spaces() + to_string(node) + " Continue(";
        result += ")\n";

        return result;
    }

    std::any visitBoolOp(BoolOp_t node) override {
        std::string result = spaces() + to_string(node) + " BoolOp(";
        result += "op=" + to_string(node->op) + ", " + "values=" +
                  to_string(node->values) + ", " + "lineno=" +
                  to_string(node->lineno) + ", " + "col_offset=" +
                  to_string(node->col_offset);
        result += ")\n";

        depth += 2;
        result += follow(node->op) + follow(node->values) +
                  follow(node->lineno) + follow(node->col_offset);
        depth -= 2;

        return result;
    }

    std::any visitBinOp(BinOp_t node) override {
        std::string result = spaces() + to_string(node) + " BinOp(";
        result += "left=" + to_string(node->left) + ", " + "op=" +
                  to_string(node->op) + ", " + "right=" +
                  to_string(node->right) + ", " + "lineno=" +
                  to_string(node->lineno) + ", " + "col_offset=" +
                  to_string(node->col_offset);
        result += ")\n";

        depth += 2;
        result += follow(node->left) + follow(node->op) + follow(node->right) +
                  follow(node->lineno) + follow(node->col_offset);
        depth -= 2;

        return result;
    }

    std::any visitLambda(Lambda_t node) override {
        std::string result = spaces() + to_string(node) + " Lambda(";
        result += "args=" + to_string(node->args) + ", " + "body=" +
                  to_string(node->body) + ", " + "lineno=" +
                  to_string(node->lineno) + ", " + "col_offset=" +
                  to_string(node->col_offset);
        result += ")\n";

        depth += 2;
        result += follow(node->args) + follow(node->body) +
                  follow(node->lineno) + follow(node->col_offset);
        depth -= 2;

        return result;
    }

    std::any visitIfExp(IfExp_t node) override {
        std::string result = spaces() + to_string(node) + " IfExp(";
        result += "test=" + to_string(node->test) + ", " + "body=" +
                  to_string(node->body) + ", " + "orelse=" +
                  to_string(node->orelse) + ", " + "lineno=" +
                  to_string(node->lineno) + ", " + "col_offset=" +
                  to_string(node->col_offset);
        result += ")\n";

        depth += 2;
        result += follow(node->test) + follow(node->body) +
                  follow(node->orelse) + follow(node->lineno) +
                  follow(node->col_offset);
        depth -= 2;

        return result;
    }

    std::any visitCompare(Compare_t node) override {
        std::string result = spaces() + to_string(node) + " Compare(";
        result += "left=" + to_string(node->left) + ", " + "ops=" +
                  to_string(node->ops) + ", " + "comparators=" +
                  to_string(node->comparators) + ", " + "lineno=" +
                  to_string(node->lineno) + ", " + "col_offset=" +
                  to_string(node->col_offset);
        result += ")\n";

        depth += 2;
        result += follow(node->left) + follow(node->ops) +
                  follow(node->comparators) + follow(node->lineno) +
                  follow(node->col_offset);
        depth -= 2;

        return result;
    }

    std::any visitCall(Call_t node) override {
        std::string result = spaces() + to_string(node) + " Call(";
        result += "func=" + to_string(node->func) + ", " + "args=" +
                  to_string(node->args) + ", " + "lineno=" +
                  to_string(node->lineno) + ", " + "col_offset=" +
                  to_string(node->col_offset);
        result += ")\n";

        depth += 2;
        result += follow(node->func) + follow(node->args) +
                  follow(node->lineno) + follow(node->col_offset);
        depth -= 2;

        return result;
    }

    std::any visitNum(Num_t node) override {
        std::string result = spaces() + to_string(node) + " Num(";
        result += "n=" + to_string(node->n) + ", " + "lineno=" +
                  to_string(node->lineno) + ", " + "col_offset=" +
                  to_string(node->col_offset);
        result += ")\n";

        depth += 2;
        result += follow(node->n) + follow(node->lineno) +
                  follow(node->col_offset);
        depth -= 2;

        return result;
    }

    std::any visitConstant(Constant_t node) override {
        std::string result = spaces() + to_string(node) + " Constant(";
        result += "value=" + to_string(node->value) + ", " + "lineno=" +
                  to_string(node->lineno) + ", " + "col_offset=" +
                  to_string(node->col_offset);
        result += ")\n";

        depth += 2;
        result += follow(node->value) + follow(node->lineno) +
                  follow(node->col_offset);
        depth -= 2;

        return result;
    }

    std::any visitAttribute(Attribute_t node) override {
        std::string result = spaces() + to_string(node) + " Attribute(";
        result += "value=" + to_string(node->value) + ", " + "attr=" +
                  to_string(node->attr) + ", " + "ctx=" + to_string(node->ctx)
                  + ", " + "lineno=" + to_string(node->lineno) + ", " +
                  "col_offset=" + to_string(node->col_offset);
        result += ")\n";

        depth += 2;
        result += follow(node->value) + follow(node->attr) + follow(node->ctx)
                  + follow(node->lineno) + follow(node->col_offset);
        depth -= 2;

        return result;
    }

    std::any visitName(Name_t node) override {
        std::string result = spaces() + to_string(node) + " Name(";
        result += "id=" + to_string(node->id) + ", " + "ctx=" +
                  to_string(node->ctx) + ", " + "lineno=" +
                  to_string(node->lineno) + ", " + "col_offset=" +
                  to_string(node->col_offset);
        result += ")\n";

        depth += 2;
        result += follow(node->id) + follow(node->ctx) + follow(node->lineno) +
                  follow(node->col_offset);
        depth -= 2;

        return result;
    }

    std::any visitExpr_Context(expr_context_t value) override {
        std::string result;
        switch (value) {
            case expr_context_t::kLoad:
                result = "Load";
                break;
            case expr_context_t::kStore:
                result = "Store";
                break;
            case expr_context_t::kDel:
                result = "Del";
                break;
            case expr_context_t::kAugLoad:
                result = "AugLoad";
                break;
            case expr_context_t::kAugStore:
                result = "AugStore";
                break;
            case expr_context_t::kParam:
                result = "Param";
                break;
        }
        return result;
    }

    std::any visitBoolop(boolop_t value) override {
        std::string result;
        switch (value) {
            case boolop_t::kAnd:
                result = "And";
                break;
            case boolop_t::kOr:
                result = "Or";
                break;
        }
        return result;
    }

    std::any visitOperator(operator_t value) override {
        std::string result;
        switch (value) {
            case operator_t::kAdd:
                result = "Add";
                break;
            case operator_t::kSub:
                result = "Sub";
                break;
            case operator_t::kMult:
                result = "Mult";
                break;
            case operator_t::kDiv:
                result = "Div";
                break;
            case operator_t::kPow:
                result = "Pow";
                break;
        }
        return result;
    }

    std::any visitCmpop(cmpop_t value) override {
        std::string result;
        switch (value) {
            case cmpop_t::kEq:
                result = "Eq";
                break;
            case cmpop_t::kNotEq:
                result = "NotEq";
                break;
            case cmpop_t::kLt:
                result = "Lt";
                break;
            case cmpop_t::kLtE:
                result = "LtE";
                break;
            case cmpop_t::kGt:
                result = "Gt";
                break;
            case cmpop_t::kGtE:
                result = "GtE";
                break;
            case cmpop_t::kIs:
                result = "Is";
                break;
            case cmpop_t::kIsNot:
                result = "IsNot";
                break;
            case cmpop_t::kIn:
                result = "In";
                break;
            case cmpop_t::kNotIn:
                result = "NotIn";
                break;
        }
        return result;
    }

    std::any visitArguments(arguments_t node) override {
        std::string result = spaces() + to_string(node) + " arguments(";
        result += "args=" + to_string(node->args) + ", " + "vararg=" +
                  to_string(node->vararg) + ", " + "kwonlyargs=" +
                  to_string(node->kwonlyargs) + ", " + "kw_defaults=" +
                  to_string(node->kw_defaults) + ", " + "kwarg=" +
                  to_string(node->kwarg) + ", " + "defaults=" +
                  to_string(node->defaults);
        result += ")\n";

        depth += 2;
        result += follow(node->args) + follow(node->vararg) +
                  follow(node->kwonlyargs) + follow(node->kw_defaults) +
                  follow(node->kwarg) + follow(node->defaults);
        depth -= 2;

        return result;
    }

    std::any visitArg(arg_t node) override {
        std::string result = spaces() + to_string(node) + " arg(";
        result += "arg=" + to_string(node->arg) + ", " + "annotation=" +
                  to_string(node->annotation) + ", " + "lineno=" +
                  to_string(node->lineno) + ", " + "col_offset=" +
                  to_string(node->col_offset);
        result += ")\n";

        depth += 2;
        result += follow(node->arg) + follow(node->annotation) +
                  follow(node->lineno) + follow(node->col_offset);
        depth -= 2;

        return result;
    }

};

std::string to_string(mod_t node) {
    ToStringVisitor string_visitor;
    return std::any_cast<std::string>(string_visitor.visit(node));
}

std::string to_string(stmt_t node) {
    ToStringVisitor string_visitor;
    return std::any_cast<std::string>(string_visitor.visit(node));
}

std::string to_string(expr_t node) {
    ToStringVisitor string_visitor;
    return std::any_cast<std::string>(string_visitor.visit(node));
}

std::string to_string(expr_context_t node) {
    ToStringVisitor string_visitor;
    return std::any_cast<std::string>(string_visitor.visit(node));
}

std::string to_string(boolop_t node) {
    ToStringVisitor string_visitor;
    return std::any_cast<std::string>(string_visitor.visit(node));
}

std::string to_string(operator_t node) {
    ToStringVisitor string_visitor;
    return std::any_cast<std::string>(string_visitor.visit(node));
}

std::string to_string(cmpop_t node) {
    ToStringVisitor string_visitor;
    return std::any_cast<std::string>(string_visitor.visit(node));
}

std::string to_string(arguments_t node) {
    ToStringVisitor string_visitor;
    return std::any_cast<std::string>(string_visitor.visit(node));
}

std::string to_string(arg_t node) {
    ToStringVisitor string_visitor;
    return std::any_cast<std::string>(string_visitor.visit(node));
}


}
