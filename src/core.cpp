#include <algorithm>
#include <ctime>
#include <iostream>
#include <map>
#include <memory>
#include <string>

#include <boost/date_time.hpp>

class Source{
    std::string url;
public:
    Source(const std::string& url): url(url){
        
    }
};

class SourceManager {
    std::map<std::string, std::string> urls;
    std::map<std::string, Source> sources;
public:
    SourceManager(const std::map<std::string, std::string>& url_config): urls(url_config){
        for(auto entry: urls){
            Source(entry.first);
        }
    }
};

namespace NextFunctorV{
    template <typename T>
    class NextFunctor;
    
    template <int i>
    class Add;
    
    template <int i>
    class NextFunctor<Add<i>>{
    public:
        static std::time_t next(std::time_t from, bool){
            return from + i;
        }
    };
    
    template <int i>
    class Subtract;
    
    template <int i>
    class NextFunctor<Subtract<i>>{
    public:
        static std::time_t next(std::time_t from, bool){
            return from - i;
        }
    };
    
    template <typename... Arguments>
    class Min;
    
    template <typename T1>
    class NextFunctor<Min<T1>>{
    public:
        static std::time_t next(std::time_t from, bool strict){
            return NextFunctor<T1>::next(from, strict);
        }
    };
    
    template <typename T1, typename... Arguments>
    class NextFunctor<Min<T1, Arguments...>>{
    public:
        static std::time_t next(std::time_t from, bool strict){
            return std::min(NextFunctor<T1>::next(from, strict), NextFunctor<Min<Arguments...>>::next(from, strict));
        }
    };
    
    template <typename... Arguments>
    class Max;
    
    template <typename T1>
    class NextFunctor<Max<T1>>{
    public:
        static std::time_t next(std::time_t from, bool strict){
            return NextFunctor<T1>::next(from, strict);
        }
    };
    
    template <typename T1, typename... Arguments>
    class NextFunctor<Max<T1, Arguments...>>{
    public:
        static std::time_t next(std::time_t from, bool strict){
            return std::max(NextFunctor<T1>::next(from, strict), NextFunctor<Max<Arguments...>>::next(from, strict));
        }
    };
}

namespace NextFunctor{
    class Base{
    public:
        Base() = default;
        virtual ~Base() = default;
        virtual std::time_t operator()(const std::time_t& from, bool strict) = 0;
    };
    
    class Add: public Base{
        std::time_t addend;
    public:
        Add(const std::time_t& addend): addend(addend){}
        virtual ~Add() = default;
        virtual std::time_t operator()(const std::time_t& from, bool) override {
            return from + addend;
        }
    };
    
    class Subtract: public Base{
        std::time_t minuend;
    public:
        Subtract(const std::time_t& minuend): minuend(minuend){}
        virtual ~Subtract() = default;
        virtual std::time_t operator()(const std::time_t& from, bool) override {
            return from - minuend;
        }
    };
    
    class Min: public Base{
        std::shared_ptr<Base> f1;
        std::shared_ptr<Base> f2;
    public:
        Min(const std::shared_ptr<Base>& f1, const std::shared_ptr<Base>& f2): f1(f1), f2(f2){}
        virtual ~Min() = default;
        virtual std::time_t operator()(const std::time_t& from, bool strict) override {
            return std::min((*f1)(from, strict), (*f2)(from, strict));
        }
    };
}


class RecordScheduler {
public:
    //void schedule(const std::string& source, const std::string& name, const NextFunctor::Base& next);  
};

int main(){
    std::map<std::string, std::string> sources_conf = {
        {"wdr5", "http://www.wdr.de/wdrlive/media/wdr5.m3u"},
        {"1live", "http://www.wdr.de/wdrlive/media/einslive.m3u"}
    };
    
    SourceManager sourceManager(sources_conf);
    
    {
        using namespace NextFunctorV;
        std::cout << NextFunctor<Max<Add<1>, Add<3>, Subtract<2>>>::next(0, false) << "\n";
        std::cout << NextFunctor<Min<Add<1>, Add<3>, Subtract<2>>>::next(0, false) << "\n";
    }
    
    {
        using namespace NextFunctor;
        {
            std::shared_ptr<Base> f1(new Add(1));
            std::shared_ptr<Base> f2(new Add(2));
            std::shared_ptr<Base> min(new Min(f1, f2));
            std::cout << (*min)(0, false) << "\n";    
        }
        
        {
            std::shared_ptr<Base> min(new Min(
                std::shared_ptr<Base>(new Add(1)), 
                std::shared_ptr<Base>(new Add(2))
            ));
            std::cout << (*min)(0, false) << "\n";
        }
    }
}