#include "next.h"

#include <algorithm>
#include <iostream>
#include <stack>

#include <boost/config/warning_disable.hpp>
#include <boost/spirit/home/x3.hpp>

#include <boost/version.hpp>

#include <boost/date_time/gregorian/gregorian.hpp>

namespace client {
    namespace parser {
        using boost::spirit::x3::ascii::space;
        using boost::spirit::x3::int_;
        using boost::spirit::x3::_attr;
        using boost::spirit::x3::lit;
        using boost::spirit::x3::rule;
        using boost::spirit::x3::blank;
        using boost::spirit::x3::symbols;

        struct months_ : symbols<boost::date_time::months_of_year> {
            months_(){
                using moy = boost::date_time::months_of_year;
                add
                    ("JAN", moy::Jan)
                    ("FEB", moy::Feb)
                    ("MAR", moy::Mar)
                    ("APR", moy::Apr)
                    ("MAY", moy::May)
                    ("JUN", moy::Jun)
                    ("JUL", moy::Jul)
                    ("AUG", moy::Aug)
                    ("SEP", moy::Sep)
                    ("OCT", moy::Oct)
                    ("NOV", moy::Nov)
                    ("DEC", moy::Dec)
                ;
            }
        } months;

        struct dayofweeks_ : symbols<boost::date_time::weekdays> {
            dayofweeks_(){
                using wd = boost::date_time::weekdays;
                add
                    ("MON", wd::Monday)
                    ("TUE", wd::Tuesday)
                    ("WED", wd::Wednesday)
                    ("THU", wd::Thursday)
                    ("FRI", wd::Friday)
                    ("SAT", wd::Saturday)
                    ("SUN", wd::Sunday)
                ;
            }
        } dayofweeks;

        auto month_f = [](auto& ctx){
            _val(ctx) = std::shared_ptr<NextFunctor::Base>(new NextFunctor::Month(_attr(ctx)));
        };

        auto dayofweek_f = [](auto& ctx){
            _val(ctx) = std::shared_ptr<NextFunctor::Base>(new NextFunctor::DayOfWeek(_attr(ctx)));
        };

        auto hour_f = [](auto& ctx){
            _val(ctx) = std::shared_ptr<NextFunctor::Base>(new NextFunctor::Hour(_attr(ctx)));
        };

        auto minute_f = [](auto& ctx){
            _val(ctx) = std::shared_ptr<NextFunctor::Base>(new NextFunctor::Minute(_attr(ctx)));
        };

        auto hm_f = [](auto& ctx){
            boost::fusion::deque<int, int> attr = _attr(ctx);
            int hour = boost::fusion::at_c<0>(attr);
            int minute = boost::fusion::at_c<1>(attr);

            std::shared_ptr<NextFunctor::Base> cond_hour(new NextFunctor::Hour(hour));
            std::shared_ptr<NextFunctor::Base> cond_minute(new NextFunctor::Minute(minute));

            std::vector<std::shared_ptr<NextFunctor::Base>> vec {cond_hour, cond_minute};
            _val(ctx) = std::shared_ptr<NextFunctor::Base>(new NextFunctor::AllOf(vec.begin(), vec.end()));
        };

        auto second_f = [](auto& ctx){
            _val(ctx) = std::shared_ptr<NextFunctor::Base>(new NextFunctor::Second(_attr(ctx)));
        };

        auto allof_f = [](auto& ctx){
            auto& vec = _attr(ctx);
            _val(ctx) = std::shared_ptr<NextFunctor::Base>(new NextFunctor::AllOf(vec.begin(), vec.end()));
        };

        auto firstof_f = [](auto& ctx){
            auto& vec = _attr(ctx);
            _val(ctx) = std::shared_ptr<NextFunctor::Base>(new NextFunctor::FirstOf(vec.begin(), vec.end()));
        };

        rule<class month, std::shared_ptr<NextFunctor::Base>> const month = "month";
        rule<class dayofweek, std::shared_ptr<NextFunctor::Base>> const dayofweek = "dayofweek";

        rule<class hour, std::shared_ptr<NextFunctor::Base>> const hour = "hour";
        rule<class minute, std::shared_ptr<NextFunctor::Base>> const minute = "minute";
        rule<class second, std::shared_ptr<NextFunctor::Base>> const second = "second";

        rule<class hour_minute, std::shared_ptr<NextFunctor::Base>> const hour_minute = "hour_minute";

        rule<class cond_proxy, std::shared_ptr<NextFunctor::Base>> const cond_proxy = "cond_proxy";
        rule<class cond, std::shared_ptr<NextFunctor::Base>> const cond = "cond";

        rule<class allof, std::shared_ptr<NextFunctor::Base>> const allof = "allof";
        rule<class firstof, std::shared_ptr<NextFunctor::Base>> const firstof = "firstof";

        auto const month_def = months[month_f];
        auto const dayofweek_def = dayofweeks[dayofweek_f];

        auto const hour_def = (int_ >> lit("H"))[hour_f];
        auto const minute_def = (int_ >> lit("M"))[minute_f];
        auto const second_def = (int_ >> lit("S"))[second_f];

        auto const hour_minute_def = (int_ >> lit(":") >> int_)[hm_f];

        auto const cond_proxy_def = month | dayofweek | hour | minute | second | hour_minute | allof | firstof;
        auto const cond_def = cond_proxy;

        auto const allof_def = (lit("(") >> (cond_def % (*blank >> '&' >> *blank)) >> lit(")"))[allof_f];
        auto const firstof_def = (lit("[") >> (cond_def % (*blank >> '|' >> *blank)) >> lit("]"))[firstof_f];

        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored  "-Wunused-parameter"
        BOOST_SPIRIT_DEFINE(month, dayofweek, hour, minute, second, hour_minute, cond_proxy, cond, allof, firstof);
        #pragma GCC diagnostic pop
    }
}

namespace NextFunctor {
    std::shared_ptr<NextFunctor::Base> Base::parse(const std::string& str){
        using client::parser::cond;

        std::string::const_iterator iter = str.cbegin();
        std::string::const_iterator end = str.cend();
        std::shared_ptr<NextFunctor::Base> result(nullptr);
        bool r(boost::spirit::x3::parse(iter, end, cond, result));

        /*
        if (r){
            std::cout << (unsigned) r << " " << *result << "\n";
        }
        */
        if (!r){
            std::cout << "parsing failed at: '" << std::string(iter, end) << "'\n";
        }

        return result;
    }

    using boost::gregorian::date;
    using boost::gregorian::days;
    using boost::gregorian::months;
    using boost::posix_time::microseconds;
    using boost::posix_time::seconds;
    using boost::posix_time::minutes;
    using boost::posix_time::hours;
    using boost::posix_time::time_duration;
    using boost::posix_time::ptime;

    template <typename T>
    constexpr long mus();

    template <>
    constexpr long mus<Second>(){
        return 1e6;
    }

    template <>
    constexpr long mus<Minute>(){
        return mus<Second>() * 60;
    }

    template <>
    constexpr long mus<Hour>(){
        return mus<Minute>() * 60;
    }

    template <typename T>
    ptime ceil(ptime t, bool force_carry){
        long rem = t.time_of_day().total_microseconds() % mus<T>();
        if(!force_carry && rem == 0)
            return t;
        else
            return t + microseconds(-rem) + microseconds(mus<T>());
    }

    template <>
    ptime ceil<DayOfMonth>(ptime t, bool force_carry){
        long rem = t.time_of_day().total_microseconds();
        if(!force_carry && rem == 0)
            return t;
        else{
            return t + microseconds(-rem) + days(1);
        }
    }

    template <>
    ptime ceil<Month>(ptime t, bool force_carry){
        if(!force_carry && t.time_of_day().total_microseconds() == 0 && t.date().day() == 1)
            return t;
        else{
            date d = (t.date() - days(t.date().day() - 1)) + months(1);
            return ptime(d, time_duration(0,0,0,0));
        }
    }

    ptime Month::operator()(const ptime& from, bool force_carry){
        if(!force_carry && from.date().month() == this->month)
            return from;

        ptime t = ceil<Month>(from, force_carry);
        auto mon_diff = this->month - t.date().month();
        if (mon_diff < 0) mon_diff += 12;
        return t + months(mon_diff);
    }

    ptime DayOfMonth::operator()(const ptime& from, bool force_carry){
        if(!force_carry && from.date().day() == this->dayofmonth)
            return from;

        ptime t = ceil<DayOfMonth>(from, force_carry);
        auto day_diff = this->dayofmonth - t.date().day();
        if (day_diff < 0){
            t = ceil<Month>(from, true);
            day_diff = this->dayofmonth - 1;
        }
        return t + days(day_diff);
    }

    ptime DayOfWeek::operator()(const ptime& from, bool force_carry){
        if(!force_carry && from.date().day_of_week() == this->dayofweek)
            return from;

        ptime t = ceil<DayOfMonth>(from, force_carry);
        date d = t.date();
        d = next_weekday(d, boost::gregorian::greg_weekday(this->dayofweek));
        return ptime(d, time_duration(0,0,0,0));
    }

    ptime Hour::operator()(const ptime& from, bool force_carry){
        if(!force_carry && from.time_of_day().hours() == this->hour)
            return from;
        ptime t = ceil<Hour>(from, force_carry);
        auto diff = mus<Hour>() * this->hour - t.time_of_day().total_microseconds();
        if(diff < 0) diff += mus<Hour>() * 24;
        return t + microseconds(diff);
    }

    ptime Minute::operator()(const ptime& from, bool force_carry){
        if(!force_carry && from.time_of_day().minutes() == this->minute)
            return from;

        ptime t = ceil<Minute>(from, force_carry);
        auto diff = mus<Minute>() * this->minute - t.time_of_day().total_microseconds() % mus<Hour>();
        if(diff < 0) diff += mus<Hour>();
        return t + microseconds(diff);
    }

    ptime Second::operator()(const ptime& from, bool force_carry){
        if(!force_carry && from.time_of_day().seconds() == this->second)
            return from;

        ptime t = ceil<Second>(from, force_carry);
        auto diff = mus<Second>() * this->second - t.time_of_day().total_microseconds() % mus<Minute>();
        if(diff < 0) diff += mus<Minute>();
        return t + microseconds(diff);
    }

    ptime AllOf::operator()(const ptime& from, bool force_carry){
        ptime t;

        if(force_carry){
            t = boost::date_time::pos_infin;
            for(auto& f: this->conditions){
                //std::cout << t << "\n";
                t = std::min(t, (*f)(from, true));
            }
            //std::cout << "final: " << t << "\n";
        }
        else{
            t = from;
        }

        ptime begin;
        while (t != begin){
            begin = t;
            for(auto& f: this->conditions){
                t = (*f)(t, false);
            }
        }
        return t;
    }

    ptime FirstOf::operator()(const ptime& from, bool force_carry){
        ptime t;

        for(auto& f: this->conditions){
            if (t == ptime()){
                t = (*f)(from, force_carry);
            }
            else{
                t = std::min(t, (*f)(from, force_carry));
            }
        }
        return t;
    }
}