#include "Runtime/DialogueVariableStore.hxx"

#include "Stream.h"

void DialogueVariableStore::Clear()
{
    mValues.clear();
}

void DialogueVariableStore::ApplyDefaults(const DialogueGraphData& graph)
{
    for (const auto& v : graph.mVariables)
    {
        if (mValues.find(v.mName) == mValues.end())
        {
            mValues[v.mName] = v.mDefault;
        }
    }
}

bool DialogueVariableStore::HasValue(const std::string& name) const
{
    return mValues.find(name) != mValues.end();
}

void DialogueVariableStore::SetBool(const std::string& name, bool value)
{
    DialogueValue v;
    v.mType = DialogueValueType::Bool;
    v.mBool = value;
    mValues[name] = v;
}

void DialogueVariableStore::SetInt(const std::string& name, int32_t value)
{
    DialogueValue v;
    v.mType = DialogueValueType::Int;
    v.mInt = value;
    mValues[name] = v;
}

void DialogueVariableStore::SetFloat(const std::string& name, float value)
{
    DialogueValue v;
    v.mType = DialogueValueType::Float;
    v.mFloat = value;
    mValues[name] = v;
}

void DialogueVariableStore::SetString(const std::string& name, const std::string& value)
{
    DialogueValue v;
    v.mType = DialogueValueType::String;
    v.mString = value;
    mValues[name] = v;
}

void DialogueVariableStore::SetValue(const std::string& name, const DialogueValue& value)
{
    mValues[name] = value;
}

bool DialogueVariableStore::GetBool(const std::string& name, bool defaultValue) const
{
    auto it = mValues.find(name);
    if (it == mValues.end()) return defaultValue;
    const DialogueValue& v = it->second;
    switch (v.mType)
    {
        case DialogueValueType::Bool:   return v.mBool;
        case DialogueValueType::Int:    return v.mInt != 0;
        case DialogueValueType::Float:  return v.mFloat != 0.0f;
        case DialogueValueType::String: return !v.mString.empty();
        default: return defaultValue;
    }
}

int32_t DialogueVariableStore::GetInt(const std::string& name, int32_t defaultValue) const
{
    auto it = mValues.find(name);
    if (it == mValues.end()) return defaultValue;
    const DialogueValue& v = it->second;
    switch (v.mType)
    {
        case DialogueValueType::Bool:  return v.mBool ? 1 : 0;
        case DialogueValueType::Int:   return v.mInt;
        case DialogueValueType::Float: return (int32_t)v.mFloat;
        default: return defaultValue;
    }
}

float DialogueVariableStore::GetFloat(const std::string& name, float defaultValue) const
{
    auto it = mValues.find(name);
    if (it == mValues.end()) return defaultValue;
    const DialogueValue& v = it->second;
    switch (v.mType)
    {
        case DialogueValueType::Bool:  return v.mBool ? 1.0f : 0.0f;
        case DialogueValueType::Int:   return (float)v.mInt;
        case DialogueValueType::Float: return v.mFloat;
        default: return defaultValue;
    }
}

std::string DialogueVariableStore::GetString(const std::string& name, const std::string& defaultValue) const
{
    auto it = mValues.find(name);
    if (it == mValues.end()) return defaultValue;
    const DialogueValue& v = it->second;
    if (v.mType == DialogueValueType::String) return v.mString;
    return defaultValue;
}

const DialogueValue* DialogueVariableStore::GetValue(const std::string& name) const
{
    auto it = mValues.find(name);
    return (it == mValues.end()) ? nullptr : &it->second;
}

void DialogueVariableStore::SaveStream(Stream& stream) const
{
    stream.WriteUint32((uint32_t)mValues.size());
    for (const auto& kv : mValues)
    {
        stream.WriteString(kv.first);
        stream.WriteUint8((uint8_t)kv.second.mType);
        stream.WriteBool(kv.second.mBool);
        stream.WriteInt32(kv.second.mInt);
        stream.WriteFloat(kv.second.mFloat);
        stream.WriteString(kv.second.mString);
    }
}

void DialogueVariableStore::LoadStream(Stream& stream)
{
    mValues.clear();
    uint32_t n = stream.ReadUint32();
    for (uint32_t i = 0; i < n; ++i)
    {
        std::string name;
        DialogueValue v;
        stream.ReadString(name);
        v.mType  = (DialogueValueType)stream.ReadUint8();
        v.mBool  = stream.ReadBool();
        v.mInt   = stream.ReadInt32();
        v.mFloat = stream.ReadFloat();
        stream.ReadString(v.mString);
        mValues.emplace(std::move(name), std::move(v));
    }
}
