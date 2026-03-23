#ifndef JSON_FETCHER_H
#define JSON_FETCHER_H

#include "json.h"
#include "../../date_time.h"
#include "../../basefunc_std.h"
#include "../../small_string.h"

namespace json
{
    class fetcher
    {
    public:
        fetcher(const Json::Value& doc_) : doc(doc_)
        {

        }

        bool Get(std::int32_t index, bool& val, bool def = false)
        {
            if (doc.isArray() && doc.size() > index)
            {
                try
                {
                    val = doc.get(index, def).asBool();
                }
                catch(std::exception& ex)
                {
                    basefunc_std::cout(ex.what(), "json::fetcher", basefunc_std::COLOR::RED_COL);
                    return false;
                }

                return true;
            }
            return false;
        }

        bool Get(std::int32_t index, std::int16_t& val, std::int16_t def = 0)
        {
            if (doc.isArray() && doc.size() > index)
            {
                try
                {
                    val = doc.get(index, def).asInt();
                }
                catch(std::exception& ex)
                {
                    basefunc_std::cout(ex.what(), "json::fetcher", basefunc_std::COLOR::RED_COL);
                    return false;
                }

                return true;
            }
            return false;
        }

        bool Get(std::int32_t index, std::int32_t& val, std::int32_t def = 0)
        {
            if (doc.isArray() && doc.size() > index)
            {
                try
                {
                    val = doc.get(index, def).asInt();
                }
                catch(std::exception& ex)
                {
                    basefunc_std::cout(ex.what(), "json::fetcher", basefunc_std::COLOR::RED_COL);
                    return false;
                }

                return true;
            }
            return false;
        }

        bool Get(std::int32_t index, std::int64_t& val, std::int64_t def = 0)
        {
            if (doc.isArray() && doc.size() > index)
            {
                try
                {
                    val = doc.get(index, Json::Int64(def)).asInt64();
                }
                catch(std::exception& ex)
                {
                    basefunc_std::cout(ex.what(), "json::fetcher", basefunc_std::COLOR::RED_COL);
                    return false;
                }

                return true;
            }
            return false;
        }

        bool Get(std::int32_t index, date_time& val, std::string format = "yyyy-MM-dd HH:mm:ss", date_time def = date_time())
        {
            if (doc.isArray() && doc.size() > index)
            {
                std::string dt;

                try
                {
                    dt = doc.get(index, "").asString();
                }
                catch(std::exception& ex)
                {
                    basefunc_std::cout(ex.what(), "json::fetcher", basefunc_std::COLOR::RED_COL);
                    return false;
                }


                if (!dt.empty())
                {
                    if (!val.parse_date_time(dt, format)) val = def;
                }
                else
                {
                    val = def;
                }

                return true;
            }
            return false;
        }

        bool Get(std::int32_t index, std::string& val, std::string def = "")
        {
            if (doc.isArray() && doc.size() > index)
            {
                try
                {
                    val = doc.get(index, def).asString();
                }
                catch(std::exception& ex)
                {
                    basefunc_std::cout(ex.what(), "json::fetcher", basefunc_std::COLOR::RED_COL);
                    return false;
                }

                return true;
            }
            return false;
        }

        bool Get(std::int32_t index, double& val, double def = 0)
        {
            if (doc.isArray() && doc.size() > index)
            {
                try
                {
                    val = doc.get(index, def).asDouble();
                }
                catch(std::exception& ex)
                {
                    basefunc_std::cout(ex.what(), "json::fetcher", basefunc_std::COLOR::RED_COL);
                    return false;
                }

                return true;
            }
            return false;
        }

        bool Get(std::int32_t index, float& val, float def = 0)
        {
            if (doc.isArray() && doc.size() > index)
            {
                try
                {
                    val = doc.get(index, def).asFloat();
                }
                catch(std::exception& ex)
                {
                    basefunc_std::cout(ex.what(), "json::fetcher", basefunc_std::COLOR::RED_COL);
                    return false;
                }

                return true;
            }
            return false;
        }

        //by name

        bool Get(const char* name, bool& val)
        {
            if (const Json::Value* v = _get(name))
            {
                if (v->isBool())
                {
                    val = v->asBool();
                    return true;
                }
                else if (v->isString())
                {
                    const std::string v_str = v->asString();
                    if (v_str.compare("true") == 0)
                    {
                        val = true;
                        return true;
                    }
                    else if (v_str.compare("false") == 0)
                    {
                        val = false;
                        return true;
                    }
                    else if (v_str.compare("0") == 0)
                    {
                        val = false;
                        return true;
                    }
                    else if (v_str.compare("1") == 0)
                    {
                        val = true;
                        return true;
                    }
                }
                else if (v->isInt())
                {
                    if (v->asInt() != 0) val = true;
                    else val = false;

                    return true;
                }
            }
            return false;
        }

        bool Get(const char* name, std::int16_t& val)
        {
            if (const Json::Value* v = _get(name))
            {
                if (v->isInt())
                {
                    val = static_cast<std::int16_t>(v->asInt());
                    return true;
                }
                else if (v->isString())
                {
                    if (basefunc_std::stoi(v->asString(), val))
                    {
                        return true;
                    }
                }
            }
            return false;
        }

        bool Get(const char* name, std::int32_t& val)
        {
            if (const Json::Value* v = _get(name))
            {
                if (v->isInt())
                {
                    val = v->asInt();
                    return true;
                }
                else if (v->isString())
                {
                    if (basefunc_std::stoi(v->asString(), val))
                    {
                        return true;
                    }
                }
            }
            return false;
        }

        bool Get(const char* name, std::int64_t& val)
        {
            if (const Json::Value* v = _get(name))
            {
                if (v->isInt64())
                {
                    val = v->asInt64();
                    return true;
                }
                else if (v->isInt())
                {
                    val = v->asInt();
                    return true;
                }
                else if (v->isString())
                {
                    if (basefunc_std::stoi(v->asString(), val))
                    {
                        return true;
                    }
                }
            }
            return false;
        }

        bool Get_md(const char* name, date_time& val)
        {
            if (const Json::Value* v = _get(name))
            {
                if (v->isMember("$date") && (*v)["$date"].isInt64())
                {
                    val = date_time::current_date_time((*v)["$date"].asLargestInt() / 1000);
                    return true;
                }
            }
            return false;
        }

        bool Get(const char* name, date_time& val, std::string format = "yyyy-MM-dd HH:mm:ss")
        {
            if (const Json::Value* v = _get(name))
            {
                if (v->isString())
                {
                    if (val.parse_date_time(v->asString(), format))
                    {
                        return true;
                    }
                }
                else if (v->isObject() && v->isMember("$date"))
                {
                    val = date_time::current_date_time(v->get("$date", 0).asInt64() / 1000);
                    return true;
                }
            }
            return false;
        }

        bool Get(const char* name, std::string& val)
        {
            if (const Json::Value* v = _get(name))
            {
                if (v->isString())
                {
                    val = v->asString();
                    return true;
                }
                else if (v->isDouble())
                {
                    val = std::to_string(v->asDouble());
                    return true;
                }
                else if (v->isInt())
                {
                    val = std::to_string(v->asInt());
                    return true;
                }
                else if (v->isInt64())
                {
                    val = std::to_string(v->asInt64());
                    return true;
                }
                else if (v->isBool())
                {
                    val = v->asBool() ? "1" : "0";
                    return true;
                }
            }
            return false;
        }

        bool Get(const char* name, small::cstring& val)
        {
            std::string _val;
            bool ok = Get(name, _val);
            val = _val;
            return ok;
        }

        bool Get(const char* name, double& val)
        {
            if (const Json::Value* v = _get(name))
            {
                if (v->isDouble())
                {
                    val = v->asDouble();
                    return true;
                }
                else if (v->isInt())
                {
                    val = static_cast<double>(v->asInt());
                    return true;
                }
                else if (v->isInt64())
                {
                    val = static_cast<double>(v->asInt64());
                    return true;
                }
                else if (v->isString())
                {
                    if (basefunc_std::stod(v->asString(), val))
                    {
                        return true;
                    }
                }
            }
            return false;
        }

        bool Get(const char* name, float& val)
        {
            if (const Json::Value* v = _get(name))
            {
                if (v->isDouble())
                {
                    val = static_cast<float>(v->asDouble());
                    return true;
                }
                else if (v->isInt())
                {
                    val = static_cast<float>(v->asInt());
                    return true;
                }
                else if (v->isInt64())
                {
                    val = static_cast<float>(v->asInt64());
                    return true;
                }
                else if (v->isString())
                {
                    if (basefunc_std::stof(v->asString(), val))
                    {
                        return true;
                    }
                }
            }
            return false;
        }

        bool Get(const char* name, std::set<std::int32_t>& val)
        {
            if (const Json::Value* v = _get(name))
            {
                if (v->isString())
                {
                    basefunc_std::get_set_from_string(v->asString(), val);
                    return true;
                }
                else if (v->isInt())
                {
                    val.emplace(v->asInt());
                    return true;
                }
            }
            return false;
        }

        bool Get(const char* name, std::set<std::int64_t>& val)
        {
            if (const Json::Value* v = _get(name))
            {
                if (v->isString())
                {
                    basefunc_std::get_set_from_string(v->asString(), val);
                    return true;
                }
                else if (v->isInt64())
                {
                    val.emplace(v->asInt64());
                    return true;
                }
                else if (v->isInt())
                {
                    val.emplace(v->asInt());
                    return true;
                }
            }
            return false;
        }

        bool Get_members(Json::Value::Members& out)
        {
            if (doc.isObject())
            {
                out = doc.getMemberNames();
                return true;
            }

            return false;
        }

    private:
        const Json::Value& doc;
        const Json::Value* _get(const char* name)
        {
            if (doc.isMember(name)) return &(doc[name]);
            return nullptr;
        }
};
}

#endif // JSON_FETCHER_H
