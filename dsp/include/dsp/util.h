#pragma once

#include <string>
#include <map>
#include <sstream>
#include <functional>
#include <thread>

namespace dsp {

    static std::map<std::thread::id, std::stringstream> threadLocalSS;

    std::stringstream& getStringStream(){
        auto id = std::this_thread::get_id();
        if(threadLocalSS.find(id) == threadLocalSS.end()){
            threadLocalSS[id] = std::stringstream{};
        }
        threadLocalSS[id].clear();
        threadLocalSS[id].str("");

        return threadLocalSS[id];
    }

    void concatenate(std::stringstream&){}

    template<typename T, typename... Args>
    void concatenate(std::stringstream& ss, T value, Args... args){
        ss << value << (sizeof...(args) > 0 ?  "_" : "");
        concatenate(ss, args...);
    }

    template<typename ReturnType, typename Delegate>
    auto memorize(Delegate&& delegate) {

        return [delegate, cache=std::map<std::string, ReturnType>{}] (auto... args) mutable {
            auto& ss = getStringStream();
            concatenate(ss, args...);
            auto key = ss.str();
            if(cache.find(key) != cache.end()){
                return cache[key];
            }
            auto result = delegate(args...);
            cache[key] = result;
            return result;
        };
    }
}