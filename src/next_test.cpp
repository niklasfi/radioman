#include "next.h"

#include <iostream>

int main(){
    using NextFunctor::Base;
    using f_ptr = std::shared_ptr<NextFunctor::Base>;
    using boost::posix_time::ptime;
    using boost::posix_time::microseconds;
    using boost::posix_time::seconds;
    using boost::posix_time::minutes;
    using boost::posix_time::hours;
    using boost::posix_time::time_duration;
    using boost::posix_time::ptime;
    using boost::gregorian::date;

    using moy = boost::date_time::months_of_year;

    {
        std::cout << "=== TEST f1 ===\n\n";

        f_ptr f(Base::parse("(8H & 37M)"));
        std::cout << *f << "\n";

        ptime d1(date(2002, moy::Jan, 10), hours(1) + seconds(5));
        ptime d1r((*f)(d1, false));

        std::cout << "d1: " << d1 << "\td1r:  " << d1r << "\n";
        assert(d1r == ptime(d1.date(), hours(8) + minutes(37)));

        ptime d2(date(2002, moy::Jan, 31), hours(8) + minutes(37) + seconds(1));
        ptime d2r((*f)(d2, false));

        std::cout << "d2: " << d2 << "\td2r:  " << d2r << "\n";
        assert(d2r == d2);

        ptime d2rr((*f)(d2, true));

        std::cout << "d2: " << d2 << "\td2rr: " << d2rr << "\n";
        assert(d2rr == ptime(date(2002, moy::Feb, 1), hours(8) + minutes(37)));

        ptime d3(date(2002, moy::Jan, 31), hours(9) + minutes(37) + seconds(1));
        ptime d3r((*f)(d3, false));

        std::cout << "d3: " << d3 << "\td3r:  " << d3r << "\n";
        assert(d3r == d2rr);

        ptime d3rr((*f)(d3, true));

        std::cout << "d3: " << d3 << "\td3rr: " << d3rr << "\n";
        assert(d3rr == d3r);

        std::cout << "OK\n\n";
    }

    {
        std::cout << "=== TEST f2 ===\n\n";

        f_ptr f(Base::parse("(WED & 13S & [(MAR & 12M) | JAN | (FRI & 17H)])"));
        std::cout << *f << "\n";

        ptime d1(date(2016, moy::Apr, 10), hours(1) + seconds(5));
        ptime d1r((*f)(d1, false));

        std::cout << "d1: " << d1 << "\td1r:  " << d1r << "\n";
        assert(d1r == ptime(date(2017, moy::Jan, 4), seconds(13)));

        ptime d2(d1r);
        ptime d2r((*f)(d2, false));

        std::cout << "d2: " << d2 << "\td2r:  " << d2r << "\n";
        assert(d2r == d2);

        ptime d2rr((*f)(d2, true));

        std::cout << "d2: " << d2 << "\td2rr: " << d2rr << "\n";
        assert(d2rr == ptime(date(2017, moy::Jan, 4), minutes(1) + seconds(13)));

        ptime d3(date(2016, moy::Feb, 27), hours(9) + minutes(37) + seconds(1));
        ptime d3r((*f)(d3, false));

        std::cout << "d3: " << d3 << "\td3r:  " << d3r << "\n";
        assert(d3r == ptime(date(2016, moy::Mar, 2), minutes(12) + seconds(13)));

        ptime d3rr((*f)(d3, true));

        std::cout << "d3: " << d3 << "\td3rr: " << d3rr << "\n";
        assert(d3rr == d3r);

        std::cout << "OK\n\n";
    }

    {
        std::cout << "=== TEST f3 ===\n\n";

        f_ptr f(Base::parse("16:30"));
        std::cout << *f << "\n";

        ptime d1(date(2016, moy::Aug, 7), hours(14) + minutes(27) + seconds(13));
        ptime d1r((*f)(d1, false));

        std::cout << "d1: " << d1 << "\td1r:  " << d1r << "\n";
        assert(d1r == ptime(date(2016, moy::Aug, 7), hours(16) + minutes(30)));

        ptime d2(d1r);
        ptime d2r((*f)(d2, false));

        std::cout << "d2: " << d2 << "\td2r:  " << d2r << "\n";
        assert(d2r == d2);

        ptime d2rr((*f)(d2, true));

        std::cout << "d2: " << d2 << "\td2rr: " << d2rr << "\n";
        assert(d2rr == ptime(date(2016, moy::Aug, 8), hours(16) + minutes(30)));

        std::cout << "OK\n\n";
    }

    {
        std::cout << "=== TEST f4 ===\n\n";

        f_ptr f(Base::parse("(WED & 14H)"));
        std::cout << *f << "\n";

        ptime d1(date(2016, moy::Aug, 31), hours(14) + minutes(5));
        ptime d1r((*f)(d1, true));

        std::cout << "d1: " << d1 << "\td1r:  " << d1r << "\n";
        assert(d1r == ptime(date(2016, moy::Sep, 7), hours(14)));

        std::cout << "OK\n\n";
    }

    {
        std::cout << "=== TEST f5 ===\n\n";

        f_ptr f(Base::parse("(WED & 14H & 5M)"));
        std::cout << *f << "\n";

        ptime d1(date(2016, moy::Aug, 31), hours(4) + minutes(5));
        ptime d1r((*f)(d1, true));

        std::cout << "d1: " << d1 << "\td1r:  " << d1r << "\n";
        assert(d1r == ptime(date(2016, moy::Aug, 31), hours(14) + minutes(5)));

        std::cout << "OK\n\n";
    }

    {
        std::cout << "=== TEST f6 ===\n\n";

        f_ptr f(Base::parse("(WED & [13H|4H] & 5M)"));
        std::cout << *f << "\n";

        ptime d1(date(2016, moy::Aug, 31), hours(4) + minutes(5));
        ptime d1r((*f)(d1, true));

        std::cout << "d1: " << d1 << "\td1r:  " << d1r << "\n";
        assert(d1r == ptime(date(2016, moy::Aug, 31), hours(13)+ minutes(5)));

        ptime d2(date(2016, moy::Aug, 31), hours(4));
        ptime d2r((*f)(d2, true));

        std::cout << "d2: " << d2 << "\td2r:  " << d2r << "\n";
        assert(d2r == ptime(date(2016, moy::Aug, 31), hours(4)+ minutes(5)));

        std::cout << "OK\n\n";
    }

    {
        std::cout << "=== TEST f7 (whitespace in parse string) ===\n\n";

        f_ptr f(Base::parse("(WED & [ 13H | 4H ] & 5M    )"));
        std::cout << *f << "\n";

        ptime d1(date(2016, moy::Aug, 31), hours(4) + minutes(5));
        ptime d1r((*f)(d1, true));

        std::cout << "d1: " << d1 << "\td1r:  " << d1r << "\n";
        assert(d1r == ptime(date(2016, moy::Aug, 31), hours(13)+ minutes(5)));

        ptime d2(date(2016, moy::Aug, 31), hours(4));
        ptime d2r((*f)(d2, true));

        std::cout << "d2: " << d2 << "\td2r:  " << d2r << "\n";
        assert(d2r == ptime(date(2016, moy::Aug, 31), hours(4)+ minutes(5)));

        std::cout << "OK\n\n";
    }
}