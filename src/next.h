#pragma once

#include <cassert>
#include <memory>
#include <vector>

#include <boost/date_time/posix_time/posix_time.hpp>

namespace NextFunctor{
    using boost::posix_time::ptime;

    class Base{
    protected:
        virtual std::ostream& ostream_operator(std::ostream& os) const = 0;
    public:
        Base() = default;
        virtual ~Base() = default;
        virtual ptime operator()(const ptime& from, bool force_carry) = 0;
        friend std::ostream& operator<<(std::ostream& os, const Base& base){
            return base.ostream_operator(os);
        }

        static std::shared_ptr<Base> parse(const std::string& str);
    };

    class Month: public Base{
    public:
        using source_t = boost::date_time::months_of_year;
        Month(const source_t& month): month(month){}
        virtual ~Month() = default;
        virtual ptime operator()(const ptime& from, bool force_carry) override;
    protected:
        virtual std::ostream& ostream_operator(std::ostream& os) const override {
            using moy = boost::date_time::months_of_year;
            switch (this->month){
                case moy::Jan:
                    os << std::string("JAN");
                    break;
                case moy::Feb:
                    os << std::string("FEB");
                    break;
                case moy::Mar:
                    os << std::string("MAR");
                    break;
                case moy::Apr:
                    os << std::string("APR");
                    break;
                case moy::May:
                    os << std::string("MAY");
                    break;
                case moy::Jun:
                    os << std::string("JUN");
                    break;
                case moy::Jul:
                    os << std::string("JUL");
                    break;
                case moy::Aug:
                    os << std::string("AUG");
                    break;
                case moy::Sep:
                    os << std::string("SEP");
                    break;
                case moy::Oct:
                    os << std::string("OCT");
                    break;
                case moy::Nov:
                    os << std::string("NOV");
                    break;
                case moy::Dec:
                    os << std::string("DEC");
                    break;
                case moy::NotAMonth:
                    os << std::string("NOT A MONTH");
                    assert(false && "NOT A MONTH");
                    break;
                case moy::NumMonths:
                    os << std::string("NUM MONTHS");
                    assert(false && "NUM MONTHS");
                    break;
            }
            return os;
        }
    private:
        source_t month;
    };

    class DayOfMonth: public Base{
    public:
        using source_t = int;
        DayOfMonth(const source_t& dayofmonth): dayofmonth(dayofmonth){
            assert(dayofmonth > 0);
            assert(dayofmonth <= 31);
        }
        virtual ~DayOfMonth() = default;
        virtual ptime operator()(const ptime& from, bool force_carry) override;
    protected:
        virtual std::ostream& ostream_operator(std::ostream& os) const override {
            return os << std::to_string(this->dayofmonth) << std::string("d");
        }
    private:
        source_t dayofmonth;
    };

    class DayOfWeek: public Base{
    public:
        using source_t = boost::date_time::weekdays;
        DayOfWeek(const source_t& dayofweek): dayofweek(dayofweek){}
        virtual ~DayOfWeek() = default;
        virtual ptime operator()(const ptime& from, bool force_carry) override;
    protected:
        virtual std::ostream& ostream_operator(std::ostream& os) const override {
            using wd = boost::date_time::weekdays;
            switch (this->dayofweek){
                case wd::Monday:
                    os << std::string("MON");
                    break;
                case wd::Tuesday:
                    os << std::string("TUE");
                    break;
                case wd::Wednesday:
                    os << std::string("WED");
                    break;
                case wd::Thursday:
                    os << std::string("THU");
                    break;
                case wd::Friday:
                    os << std::string("FRI");
                    break;
                case wd::Saturday:
                    os << std::string("SAT");
                    break;
                case wd::Sunday:
                    os << std::string("SUN");
                    break;
            };
            return os;
        }
    private:
        source_t dayofweek;
    };

    class Hour: public Base{
    public:
        using source_t = int;
        Hour(const source_t& hour): hour(hour){
            assert(hour >= 0);
            assert(hour < 24);
        }
        virtual ~Hour() = default;
        virtual ptime operator()(const ptime& from, bool) override;
    protected:
        virtual std::ostream& ostream_operator(std::ostream& os) const override {
            return os << std::to_string(this->hour) << std::string("H");
        }
    private:
        source_t hour;
    };

    class Minute: public Base{
    public:
        using source_t = int;
        Minute(const source_t& minute): minute(minute){
            assert(minute >= 0);
            assert(minute < 60);
        }
        virtual ~Minute() = default;
        virtual ptime operator()(const ptime& from, bool force_carry) override;
    protected:
        virtual std::ostream& ostream_operator(std::ostream& os) const override {
            return os << std::to_string(this->minute) << std::string("M");
        }
    private:
        source_t minute;
    };

    class Second: public Base{
    public:
        using source_t = int;
        Second(const source_t& second): second(second){
            assert(second >= 0);
            assert(second < 60);
        }
        virtual ~Second() = default;
        virtual ptime operator()(const ptime& from, bool force_carry) override;
    protected:
        virtual std::ostream& ostream_operator(std::ostream& os) const override {
            return os << std::to_string(this->second) << std::string("S");
        }
    private:
        source_t second;
    };

    class AllOf: public Base{
    public:
        using element_t = std::shared_ptr<Base>;
        using source_t = std::vector<element_t>;
        template <typename InputIterator>
        AllOf(InputIterator begin, InputIterator end): conditions(begin, end){
            assert(conditions.size() > 0);
        }
        virtual ~AllOf() = default;
        virtual ptime operator()(const ptime& from, bool force_carry) override;
    protected:
        virtual std::ostream& ostream_operator(std::ostream& os) const override {
            auto it = this->conditions.begin();
            os << std::string("(") << **(it++);
            for(; it < this->conditions.end(); ++it){
                os << std::string(" & ") << **it;
            }
            return os << std::string(")");
        }
    private:
        source_t conditions;
    };

    class FirstOf: public Base{
    public:
        using element_t = std::shared_ptr<Base>;
        using source_t = std::vector<element_t>;
        template <typename InputIterator>
        FirstOf(InputIterator begin, InputIterator end): conditions(begin, end){
            assert(conditions.size() > 0);
        }
        virtual ~FirstOf() = default;
        virtual ptime operator()(const ptime& from, bool force_carry) override;
    protected:
        virtual std::ostream& ostream_operator(std::ostream& os) const override {
            auto it = this->conditions.begin();
            os << std::string("[") << **(it++);
            for(; it < this->conditions.end(); ++it){
                os << std::string(" | ") << **it;
            }
            return os << std::string("]");
        }
    private:
        source_t conditions;
    };
}
