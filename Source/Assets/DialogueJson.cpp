#include "Assets/DialogueJson.hxx"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

// Minimal JSON parser tailored to the DialogueGraphData schema. Exception-free
// and console-safe. Documented schema lives in Docs/02-DialogueAssets.md.

namespace DialogueAddon
{

namespace
{

// ---- Generic JSON value ----------------------------------------------------

enum class JsonType : uint8_t
{
    Null,
    Bool,
    Number,
    String,
    Array,
    Object
};

struct JsonValue;

struct JsonValue
{
    JsonType    type = JsonType::Null;
    bool        b    = false;
    double      n    = 0.0;
    std::string s;
    std::vector<JsonValue>                          arr;
    std::vector<std::pair<std::string, JsonValue>>  obj;

    bool IsObject() const { return type == JsonType::Object; }
    bool IsArray()  const { return type == JsonType::Array;  }
    bool IsString() const { return type == JsonType::String; }
    bool IsNumber() const { return type == JsonType::Number; }
    bool IsBool()   const { return type == JsonType::Bool;   }
    bool IsNull()   const { return type == JsonType::Null;   }

    const JsonValue* Find(const char* key) const
    {
        if (type != JsonType::Object) return nullptr;
        for (const auto& kv : obj)
        {
            if (kv.first == key) return &kv.second;
        }
        return nullptr;
    }

    const std::string& AsString(const std::string& def = std::string()) const
    {
        return (type == JsonType::String) ? s : def;
    }
    int32_t AsInt(int32_t def = 0) const
    {
        return (type == JsonType::Number) ? (int32_t)n : def;
    }
    float AsFloat(float def = 0.0f) const
    {
        return (type == JsonType::Number) ? (float)n : def;
    }
    bool AsBool(bool def = false) const
    {
        return (type == JsonType::Bool) ? b : def;
    }
};

// ---- Tokenizing recursive-descent parser -----------------------------------

class JsonParser
{
public:
    JsonParser(const char* data, size_t size)
        : mData(data), mEnd(data + size), mP(data) {}

    bool Parse(JsonValue& out, std::string& outError)
    {
        SkipWs();
        if (!ParseValue(out, outError)) return false;
        SkipWs();
        if (mP != mEnd)
        {
            // Allow optional trailing whitespace; anything else is an error.
            outError = ErrorAt("trailing characters after root value");
            return false;
        }
        return true;
    }

private:
    const char* mData;
    const char* mEnd;
    const char* mP;

    void SkipWs()
    {
        while (mP < mEnd)
        {
            char c = *mP;
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') { ++mP; continue; }
            // Allow // line comments and /* */ block comments — handy for
            // hand-authored fixture files. Strict JSON would reject these.
            if (c == '/' && mP + 1 < mEnd)
            {
                if (mP[1] == '/')
                {
                    mP += 2;
                    while (mP < mEnd && *mP != '\n') ++mP;
                    continue;
                }
                if (mP[1] == '*')
                {
                    mP += 2;
                    while (mP + 1 < mEnd && !(mP[0] == '*' && mP[1] == '/')) ++mP;
                    if (mP + 1 < mEnd) mP += 2; else mP = mEnd;
                    continue;
                }
            }
            break;
        }
    }

    std::string ErrorAt(const char* msg)
    {
        size_t off = (size_t)(mP - mData);
        // Recover line/col so authors can find the error fast.
        size_t line = 1, col = 1;
        for (const char* p = mData; p < mP; ++p)
        {
            if (*p == '\n') { ++line; col = 1; } else { ++col; }
        }
        char buf[256];
        std::snprintf(buf, sizeof(buf), "JSON parse error at line %zu col %zu (offset %zu): %s",
                      line, col, off, msg);
        return std::string(buf);
    }

    bool ParseValue(JsonValue& out, std::string& outError)
    {
        SkipWs();
        if (mP >= mEnd) { outError = ErrorAt("unexpected end of input"); return false; }

        char c = *mP;
        if (c == '{') return ParseObject(out, outError);
        if (c == '[') return ParseArray(out, outError);
        if (c == '"') { out.type = JsonType::String; return ParseString(out.s, outError); }
        if (c == 't' || c == 'f') return ParseBool(out, outError);
        if (c == 'n') return ParseNull(out, outError);
        if (c == '-' || (c >= '0' && c <= '9')) return ParseNumber(out, outError);

        outError = ErrorAt("unexpected character");
        return false;
    }

    bool ParseObject(JsonValue& out, std::string& outError)
    {
        out.type = JsonType::Object;
        ++mP; // consume '{'
        SkipWs();
        if (mP < mEnd && *mP == '}') { ++mP; return true; }

        while (mP < mEnd)
        {
            SkipWs();
            if (mP >= mEnd || *mP != '"') { outError = ErrorAt("expected string key"); return false; }
            std::string key;
            if (!ParseString(key, outError)) return false;

            SkipWs();
            if (mP >= mEnd || *mP != ':') { outError = ErrorAt("expected ':'"); return false; }
            ++mP;

            JsonValue val;
            if (!ParseValue(val, outError)) return false;
            out.obj.emplace_back(std::move(key), std::move(val));

            SkipWs();
            if (mP >= mEnd) { outError = ErrorAt("unexpected end inside object"); return false; }
            if (*mP == ',') { ++mP; continue; }
            if (*mP == '}') { ++mP; return true; }
            outError = ErrorAt("expected ',' or '}'");
            return false;
        }
        outError = ErrorAt("unterminated object");
        return false;
    }

    bool ParseArray(JsonValue& out, std::string& outError)
    {
        out.type = JsonType::Array;
        ++mP; // consume '['
        SkipWs();
        if (mP < mEnd && *mP == ']') { ++mP; return true; }

        while (mP < mEnd)
        {
            JsonValue val;
            if (!ParseValue(val, outError)) return false;
            out.arr.emplace_back(std::move(val));

            SkipWs();
            if (mP >= mEnd) { outError = ErrorAt("unexpected end inside array"); return false; }
            if (*mP == ',') { ++mP; continue; }
            if (*mP == ']') { ++mP; return true; }
            outError = ErrorAt("expected ',' or ']'");
            return false;
        }
        outError = ErrorAt("unterminated array");
        return false;
    }

    bool ParseString(std::string& out, std::string& outError)
    {
        if (mP >= mEnd || *mP != '"') { outError = ErrorAt("expected '\"'"); return false; }
        ++mP;
        out.clear();
        while (mP < mEnd)
        {
            char c = *mP++;
            if (c == '"') return true;
            if (c == '\\')
            {
                if (mP >= mEnd) { outError = ErrorAt("bad escape"); return false; }
                char esc = *mP++;
                switch (esc)
                {
                    case '"':  out.push_back('"');  break;
                    case '\\': out.push_back('\\'); break;
                    case '/':  out.push_back('/');  break;
                    case 'b':  out.push_back('\b'); break;
                    case 'f':  out.push_back('\f'); break;
                    case 'n':  out.push_back('\n'); break;
                    case 'r':  out.push_back('\r'); break;
                    case 't':  out.push_back('\t'); break;
                    case 'u':
                    {
                        // Minimal \uXXXX support: BMP only, encode as UTF-8.
                        if (mP + 4 > mEnd) { outError = ErrorAt("short \\u escape"); return false; }
                        uint32_t cp = 0;
                        for (int i = 0; i < 4; ++i)
                        {
                            char h = *mP++;
                            uint32_t v = 0;
                            if      (h >= '0' && h <= '9') v = (uint32_t)(h - '0');
                            else if (h >= 'a' && h <= 'f') v = (uint32_t)(h - 'a' + 10);
                            else if (h >= 'A' && h <= 'F') v = (uint32_t)(h - 'A' + 10);
                            else { outError = ErrorAt("bad hex in \\u escape"); return false; }
                            cp = (cp << 4) | v;
                        }
                        if (cp < 0x80)
                        {
                            out.push_back((char)cp);
                        }
                        else if (cp < 0x800)
                        {
                            out.push_back((char)(0xC0 | (cp >> 6)));
                            out.push_back((char)(0x80 | (cp & 0x3F)));
                        }
                        else
                        {
                            out.push_back((char)(0xE0 | (cp >> 12)));
                            out.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
                            out.push_back((char)(0x80 | (cp & 0x3F)));
                        }
                        break;
                    }
                    default: outError = ErrorAt("unknown escape"); return false;
                }
            }
            else
            {
                out.push_back(c);
            }
        }
        outError = ErrorAt("unterminated string");
        return false;
    }

    bool ParseBool(JsonValue& out, std::string& outError)
    {
        if (mP + 4 <= mEnd && std::memcmp(mP, "true", 4) == 0)
        {
            out.type = JsonType::Bool; out.b = true; mP += 4; return true;
        }
        if (mP + 5 <= mEnd && std::memcmp(mP, "false", 5) == 0)
        {
            out.type = JsonType::Bool; out.b = false; mP += 5; return true;
        }
        outError = ErrorAt("invalid literal");
        return false;
    }

    bool ParseNull(JsonValue& out, std::string& outError)
    {
        if (mP + 4 <= mEnd && std::memcmp(mP, "null", 4) == 0)
        {
            out.type = JsonType::Null; mP += 4; return true;
        }
        outError = ErrorAt("invalid literal");
        return false;
    }

    bool ParseNumber(JsonValue& out, std::string& outError)
    {
        const char* start = mP;
        if (*mP == '-') ++mP;
        while (mP < mEnd && *mP >= '0' && *mP <= '9') ++mP;
        if (mP < mEnd && *mP == '.')
        {
            ++mP;
            while (mP < mEnd && *mP >= '0' && *mP <= '9') ++mP;
        }
        if (mP < mEnd && (*mP == 'e' || *mP == 'E'))
        {
            ++mP;
            if (mP < mEnd && (*mP == '+' || *mP == '-')) ++mP;
            while (mP < mEnd && *mP >= '0' && *mP <= '9') ++mP;
        }
        if (mP == start) { outError = ErrorAt("invalid number"); return false; }

        // strtod is locale-aware; for portability we copy into a temp buffer
        // and call strtod with C locale assumptions held by the host.
        std::string tmp(start, (size_t)(mP - start));
        char* endp = nullptr;
        double v = std::strtod(tmp.c_str(), &endp);
        if (endp == tmp.c_str()) { outError = ErrorAt("number conversion failed"); return false; }
        out.type = JsonType::Number;
        out.n = v;
        return true;
    }
};

// ---- Schema mapping -------------------------------------------------------

DialogueNodeType ParseNodeType(const std::string& s)
{
    if (s == "line")         return DialogueNodeType::Line;
    if (s == "choice")       return DialogueNodeType::Choice;
    if (s == "branch")       return DialogueNodeType::Branch;
    if (s == "event")        return DialogueNodeType::Event;
    if (s == "set_variable") return DialogueNodeType::SetVariable;
    if (s == "set")          return DialogueNodeType::SetVariable; // alias
    if (s == "jump")         return DialogueNodeType::Jump;
    if (s == "end")          return DialogueNodeType::End;
    return DialogueNodeType::Line;
}

const char* NodeTypeName(DialogueNodeType t)
{
    switch (t)
    {
        case DialogueNodeType::Line:        return "line";
        case DialogueNodeType::Choice:      return "choice";
        case DialogueNodeType::Branch:      return "branch";
        case DialogueNodeType::Event:       return "event";
        case DialogueNodeType::SetVariable: return "set_variable";
        case DialogueNodeType::Jump:        return "jump";
        case DialogueNodeType::End:         return "end";
        default:                            return "line";
    }
}

DialogueValueType ParseValueType(const std::string& s)
{
    if (s == "bool")   return DialogueValueType::Bool;
    if (s == "int")    return DialogueValueType::Int;
    if (s == "float")  return DialogueValueType::Float;
    if (s == "string") return DialogueValueType::String;
    return DialogueValueType::Bool;
}

const char* ValueTypeName(DialogueValueType t)
{
    switch (t)
    {
        case DialogueValueType::Bool:   return "bool";
        case DialogueValueType::Int:    return "int";
        case DialogueValueType::Float:  return "float";
        case DialogueValueType::String: return "string";
        default:                        return "bool";
    }
}

DialogueConditionOp ParseConditionOp(const std::string& s)
{
    if (s == "exists")     return DialogueConditionOp::Exists;
    if (s == "not_exists") return DialogueConditionOp::NotExists;
    if (s == "equals" || s == "eq")  return DialogueConditionOp::Equals;
    if (s == "not_equals" || s == "ne") return DialogueConditionOp::NotEquals;
    if (s == "greater" || s == "gt") return DialogueConditionOp::Greater;
    if (s == "ge")                   return DialogueConditionOp::GreaterOrEqual;
    if (s == "less" || s == "lt")    return DialogueConditionOp::Less;
    if (s == "le")                   return DialogueConditionOp::LessOrEqual;
    return DialogueConditionOp::Equals;
}

const char* ConditionOpName(DialogueConditionOp o)
{
    switch (o)
    {
        case DialogueConditionOp::Exists:         return "exists";
        case DialogueConditionOp::NotExists:      return "not_exists";
        case DialogueConditionOp::Equals:         return "equals";
        case DialogueConditionOp::NotEquals:      return "not_equals";
        case DialogueConditionOp::Greater:        return "greater";
        case DialogueConditionOp::GreaterOrEqual: return "ge";
        case DialogueConditionOp::Less:           return "less";
        case DialogueConditionOp::LessOrEqual:    return "le";
        default:                                  return "equals";
    }
}

DialogueVarOp ParseVarOp(const std::string& s)
{
    if (s == "set")    return DialogueVarOp::Set;
    if (s == "add")    return DialogueVarOp::Add;
    if (s == "sub")    return DialogueVarOp::Sub;
    if (s == "toggle") return DialogueVarOp::Toggle;
    return DialogueVarOp::None;
}

const char* VarOpName(DialogueVarOp o)
{
    switch (o)
    {
        case DialogueVarOp::Set:    return "set";
        case DialogueVarOp::Add:    return "add";
        case DialogueVarOp::Sub:    return "sub";
        case DialogueVarOp::Toggle: return "toggle";
        default:                    return "none";
    }
}

// Coerce a JsonValue (bool/number/string) into a DialogueValue. Tag the type
// based on what the JSON literal actually was so the runtime store can do
// per-type comparisons without surprising auto-conversion.
DialogueValue ConvertValue(const JsonValue& v)
{
    DialogueValue out;
    if (v.IsBool())
    {
        out.mType = DialogueValueType::Bool;
        out.mBool = v.b;
    }
    else if (v.IsNumber())
    {
        // Treat integer-looking numbers as Int, otherwise Float. This keeps
        // round-trip stable for hand-authored fixtures (e.g. `5` vs `5.0`).
        double rounded = (double)(int32_t)v.n;
        if (rounded == v.n)
        {
            out.mType = DialogueValueType::Int;
            out.mInt  = (int32_t)v.n;
        }
        else
        {
            out.mType = DialogueValueType::Float;
            out.mFloat = (float)v.n;
        }
    }
    else if (v.IsString())
    {
        out.mType = DialogueValueType::String;
        out.mString = v.s;
    }
    return out;
}

void ConvertConditionList(const JsonValue* arr, std::vector<DialogueConditionData>& out)
{
    if (arr == nullptr || !arr->IsArray()) return;
    for (const auto& el : arr->arr)
    {
        if (!el.IsObject()) continue;
        DialogueConditionData c;
        if (auto* p = el.Find("var"))    c.mVariableName = p->AsString();
        if (auto* p = el.Find("op"))     c.mOp = ParseConditionOp(p->AsString("equals"));
        if (auto* p = el.Find("value"))  c.mValue = ConvertValue(*p);
        out.emplace_back(std::move(c));
    }
}

void ConvertVariableOp(const JsonValue* obj, DialogueVariableOpData& out)
{
    if (obj == nullptr || !obj->IsObject()) return;
    if (auto* p = obj->Find("op"))    out.mOp = ParseVarOp(p->AsString("none"));
    if (auto* p = obj->Find("var"))   out.mVariableName = p->AsString();
    if (auto* p = obj->Find("value")) out.mValue = ConvertValue(*p);
}

void ConvertStringList(const JsonValue* arr, std::vector<std::string>& out)
{
    if (arr == nullptr || !arr->IsArray()) return;
    for (const auto& el : arr->arr)
    {
        if (el.IsString()) out.push_back(el.s);
    }
}

bool ConvertGraph(const JsonValue& root, DialogueGraphData& out, std::string& outError)
{
    if (!root.IsObject()) { outError = "root must be an object"; return false; }

    if (auto* p = root.Find("start")) out.mStartNodeId = p->AsString();

    if (auto* sp = root.Find("speakers"))
    {
        if (sp->IsArray())
        {
            for (const auto& el : sp->arr)
            {
                if (!el.IsObject()) continue;
                DialogueSpeakerDef s;
                if (auto* p = el.Find("id"))        s.mId   = p->AsString();
                if (auto* p = el.Find("name"))      s.mName = p->AsString();
                if (auto* p = el.Find("portrait"))  s.mDefaultPortraitAssetName = p->AsString();
                if (auto* p = el.Find("voice"))     s.mDefaultVoiceAssetName    = p->AsString();
                out.mSpeakers.emplace_back(std::move(s));
            }
        }
    }

    if (auto* vs = root.Find("variables"))
    {
        if (vs->IsArray())
        {
            for (const auto& el : vs->arr)
            {
                if (!el.IsObject()) continue;
                DialogueVariableDef v;
                if (auto* p = el.Find("name")) v.mName = p->AsString();
                if (auto* p = el.Find("type")) v.mType = ParseValueType(p->AsString("bool"));
                if (auto* p = el.Find("default"))
                {
                    v.mDefault = ConvertValue(*p);
                    // If declared type doesn't match the default's literal, prefer
                    // the declared type and reset the value bytes to zero.
                    if (v.mDefault.mType != v.mType)
                    {
                        v.mDefault.mType = v.mType;
                        v.mDefault.mBool = false;
                        v.mDefault.mInt = 0;
                        v.mDefault.mFloat = 0.0f;
                        v.mDefault.mString.clear();
                    }
                }
                else
                {
                    v.mDefault.mType = v.mType;
                }
                out.mVariables.emplace_back(std::move(v));
            }
        }
    }

    if (auto* nodes = root.Find("nodes"))
    {
        if (nodes->IsArray())
        {
            for (const auto& el : nodes->arr)
            {
                if (!el.IsObject()) continue;
                DialogueNodeData n;
                if (auto* p = el.Find("id"))             n.mId = p->AsString();
                if (auto* p = el.Find("type"))           n.mType = ParseNodeType(p->AsString("line"));
                if (auto* p = el.Find("speaker"))        n.mSpeakerId = p->AsString();
                if (auto* p = el.Find("text"))           n.mText = p->AsString();
                if (auto* p = el.Find("locKey"))         n.mLocalizationKey = p->AsString();
                if (auto* p = el.Find("portrait"))       n.mPortraitAssetName = p->AsString();
                if (auto* p = el.Find("voice"))          n.mVoiceAssetName = p->AsString();
                if (auto* p = el.Find("eventName"))      n.mEventName = p->AsString();
                if (auto* p = el.Find("jumpTargetId"))   n.mJumpTargetId = p->AsString();

                ConvertStringList(el.Find("tags"), n.mTags);
                ConvertConditionList(el.Find("conditions"), n.mConditions);

                if (auto* ch = el.Find("choices"))
                {
                    if (ch->IsArray())
                    {
                        for (const auto& cel : ch->arr)
                        {
                            if (!cel.IsObject()) continue;
                            DialogueChoiceData c;
                            if (auto* p = cel.Find("id"))     c.mId = p->AsString();
                            if (auto* p = cel.Find("text"))   c.mText = p->AsString();
                            if (auto* p = cel.Find("locKey"))  c.mLocalizationKey = p->AsString();
                            if (auto* p = cel.Find("target")) c.mTargetNodeId = p->AsString();
                            ConvertStringList(cel.Find("tags"), c.mTags);
                            ConvertConditionList(cel.Find("conditions"), c.mConditions);
                            n.mChoices.emplace_back(std::move(c));
                        }
                    }
                }

                ConvertVariableOp(el.Find("variableOp"), n.mVariableOp);
                out.mNodes.emplace_back(std::move(n));
            }
        }
    }

    if (auto* links = root.Find("links"))
    {
        if (links->IsArray())
        {
            for (const auto& el : links->arr)
            {
                if (!el.IsObject()) continue;
                DialogueLinkData l;
                if (auto* p = el.Find("from")) l.mFromNodeId = p->AsString();
                if (auto* p = el.Find("to"))   l.mToNodeId = p->AsString();
                out.mLinks.emplace_back(std::move(l));
            }
        }
    }

    return true;
}

// ---- Writer -----------------------------------------------------------------

void EscapeString(const std::string& in, std::string& out)
{
    out.push_back('"');
    for (char c : in)
    {
        switch (c)
        {
            case '"':  out.append("\\\""); break;
            case '\\': out.append("\\\\"); break;
            case '\b': out.append("\\b");  break;
            case '\f': out.append("\\f");  break;
            case '\n': out.append("\\n");  break;
            case '\r': out.append("\\r");  break;
            case '\t': out.append("\\t");  break;
            default:
                if ((unsigned char)c < 0x20)
                {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", (unsigned)(unsigned char)c);
                    out.append(buf);
                }
                else
                {
                    out.push_back(c);
                }
                break;
        }
    }
    out.push_back('"');
}

void Indent(std::string& out, int depth)
{
    for (int i = 0; i < depth; ++i) out.append("  ");
}

void WriteValue(std::string& out, const DialogueValue& v)
{
    switch (v.mType)
    {
        case DialogueValueType::Bool:   out.append(v.mBool ? "true" : "false"); break;
        case DialogueValueType::Int:
        {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%d", v.mInt);
            out.append(buf);
            break;
        }
        case DialogueValueType::Float:
        {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%g", v.mFloat);
            out.append(buf);
            break;
        }
        case DialogueValueType::String: EscapeString(v.mString, out); break;
        default: out.append("null"); break;
    }
}

void WriteConditionList(std::string& out, const std::vector<DialogueConditionData>& list, int depth)
{
    out.append("[");
    for (size_t i = 0; i < list.size(); ++i)
    {
        const auto& c = list[i];
        if (i) out.append(", ");
        out.append("{ \"var\": ");
        EscapeString(c.mVariableName, out);
        out.append(", \"op\": \"");
        out.append(ConditionOpName(c.mOp));
        out.append("\", \"value\": ");
        WriteValue(out, c.mValue);
        out.append(" }");
    }
    out.append("]");
    (void)depth;
}

void WriteStringList(std::string& out, const std::vector<std::string>& list)
{
    out.append("[");
    for (size_t i = 0; i < list.size(); ++i)
    {
        if (i) out.append(", ");
        EscapeString(list[i], out);
    }
    out.append("]");
}

} // namespace

bool ParseDialogueJson(const char* data, size_t size,
                       DialogueGraphData& outGraph,
                       std::string& outError)
{
    JsonValue root;
    JsonParser parser(data, size);
    if (!parser.Parse(root, outError)) return false;

    outGraph = DialogueGraphData{};
    return ConvertGraph(root, outGraph, outError);
}

bool ReadDialogueJsonFile(const std::string& path,
                          DialogueGraphData& outGraph,
                          std::string& outError)
{
    FILE* f = std::fopen(path.c_str(), "rb");
    if (f == nullptr) { outError = "could not open file: " + path; return false; }

    std::fseek(f, 0, SEEK_END);
    long sz = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);

    if (sz <= 0)
    {
        std::fclose(f);
        outError = "empty or unreadable file: " + path;
        return false;
    }

    std::vector<char> buf((size_t)sz);
    size_t got = std::fread(buf.data(), 1, (size_t)sz, f);
    std::fclose(f);
    if (got != (size_t)sz) { outError = "short read on " + path; return false; }

    return ParseDialogueJson(buf.data(), buf.size(), outGraph, outError);
}

std::string WriteDialogueJson(const DialogueGraphData& g)
{
    std::string s;
    s.append("{\n");
    Indent(s, 1); s.append("\"start\": ");
    EscapeString(g.mStartNodeId, s);
    s.append(",\n");

    Indent(s, 1); s.append("\"speakers\": [");
    for (size_t i = 0; i < g.mSpeakers.size(); ++i)
    {
        const auto& sp = g.mSpeakers[i];
        s.append("\n");
        Indent(s, 2);
        s.append("{ \"id\": "); EscapeString(sp.mId, s);
        s.append(", \"name\": "); EscapeString(sp.mName, s);
        if (!sp.mDefaultPortraitAssetName.empty())
        {
            s.append(", \"portrait\": "); EscapeString(sp.mDefaultPortraitAssetName, s);
        }
        if (!sp.mDefaultVoiceAssetName.empty())
        {
            s.append(", \"voice\": "); EscapeString(sp.mDefaultVoiceAssetName, s);
        }
        s.append(" }");
        if (i + 1 < g.mSpeakers.size()) s.append(",");
    }
    if (!g.mSpeakers.empty()) { s.append("\n"); Indent(s, 1); }
    s.append("],\n");

    Indent(s, 1); s.append("\"variables\": [");
    for (size_t i = 0; i < g.mVariables.size(); ++i)
    {
        const auto& v = g.mVariables[i];
        s.append("\n");
        Indent(s, 2);
        s.append("{ \"name\": "); EscapeString(v.mName, s);
        s.append(", \"type\": \""); s.append(ValueTypeName(v.mType)); s.append("\"");
        s.append(", \"default\": "); WriteValue(s, v.mDefault);
        s.append(" }");
        if (i + 1 < g.mVariables.size()) s.append(",");
    }
    if (!g.mVariables.empty()) { s.append("\n"); Indent(s, 1); }
    s.append("],\n");

    Indent(s, 1); s.append("\"nodes\": [");
    for (size_t i = 0; i < g.mNodes.size(); ++i)
    {
        const auto& n = g.mNodes[i];
        s.append("\n");
        Indent(s, 2); s.append("{ \"id\": "); EscapeString(n.mId, s);
        s.append(", \"type\": \""); s.append(NodeTypeName(n.mType)); s.append("\"");
        if (!n.mSpeakerId.empty())          { s.append(", \"speaker\": "); EscapeString(n.mSpeakerId, s); }
        if (!n.mText.empty())               { s.append(", \"text\": "); EscapeString(n.mText, s); }
        if (!n.mLocalizationKey.empty())    { s.append(", \"locKey\": "); EscapeString(n.mLocalizationKey, s); }
        if (!n.mPortraitAssetName.empty())  { s.append(", \"portrait\": "); EscapeString(n.mPortraitAssetName, s); }
        if (!n.mVoiceAssetName.empty())     { s.append(", \"voice\": "); EscapeString(n.mVoiceAssetName, s); }
        if (!n.mEventName.empty())          { s.append(", \"eventName\": "); EscapeString(n.mEventName, s); }
        if (!n.mJumpTargetId.empty())       { s.append(", \"jumpTargetId\": "); EscapeString(n.mJumpTargetId, s); }
        if (!n.mTags.empty())
        {
            s.append(", \"tags\": "); WriteStringList(s, n.mTags);
        }
        if (!n.mConditions.empty())
        {
            s.append(", \"conditions\": "); WriteConditionList(s, n.mConditions, 0);
        }
        if (!n.mChoices.empty())
        {
            s.append(", \"choices\": [");
            for (size_t ci = 0; ci < n.mChoices.size(); ++ci)
            {
                const auto& c = n.mChoices[ci];
                s.append("\n");
                Indent(s, 3);
                s.append("{ \"id\": "); EscapeString(c.mId, s);
                s.append(", \"text\": "); EscapeString(c.mText, s);
                if (!c.mLocalizationKey.empty())
                {
                    s.append(", \"locKey\": "); EscapeString(c.mLocalizationKey, s);
                }
                s.append(", \"target\": "); EscapeString(c.mTargetNodeId, s);
                if (!c.mTags.empty())
                {
                    s.append(", \"tags\": "); WriteStringList(s, c.mTags);
                }
                if (!c.mConditions.empty())
                {
                    s.append(", \"conditions\": "); WriteConditionList(s, c.mConditions, 0);
                }
                s.append(" }");
                if (ci + 1 < n.mChoices.size()) s.append(",");
            }
            s.append("\n"); Indent(s, 2); s.append("]");
        }
        if (n.mVariableOp.mOp != DialogueVarOp::None)
        {
            s.append(", \"variableOp\": { \"op\": \"");
            s.append(VarOpName(n.mVariableOp.mOp));
            s.append("\", \"var\": ");
            EscapeString(n.mVariableOp.mVariableName, s);
            s.append(", \"value\": ");
            WriteValue(s, n.mVariableOp.mValue);
            s.append(" }");
        }
        s.append(" }");
        if (i + 1 < g.mNodes.size()) s.append(",");
    }
    if (!g.mNodes.empty()) { s.append("\n"); Indent(s, 1); }
    s.append("]");

    if (!g.mLinks.empty())
    {
        s.append(",\n");
        Indent(s, 1); s.append("\"links\": [");
        for (size_t i = 0; i < g.mLinks.size(); ++i)
        {
            const auto& l = g.mLinks[i];
            s.append("\n");
            Indent(s, 2);
            s.append("{ \"from\": "); EscapeString(l.mFromNodeId, s);
            s.append(", \"to\": ");   EscapeString(l.mToNodeId, s);
            s.append(" }");
            if (i + 1 < g.mLinks.size()) s.append(",");
        }
        s.append("\n"); Indent(s, 1); s.append("]");
    }

    s.append("\n}\n");
    return s;
}

} // namespace DialogueAddon
