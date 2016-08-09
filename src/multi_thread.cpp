#include <curl/curl.h>

#include <iostream>
#include <sstream>
#include <fstream>

#include <thread>
#include <vector>
#include <queue>

#include "next.h"

#include <boost/filesystem.hpp>

class DlData_direct {
public:
    std::ofstream destination;
    boost::posix_time::ptime until;
};

size_t write_callback_direct(char *ptr, size_t size, size_t nmemb, void *userdata){
    DlData_direct* dlData_direct = static_cast<DlData_direct*>(userdata);

    dlData_direct->destination.write(ptr, size * nmemb);

    return boost::posix_time::microsec_clock::local_time() <= dlData_direct->until ? size * nmemb : 0;
}

void download_direct(const std::string& url, const std::string& destination, boost::posix_time::ptime until){
    CURL* easyhandle = curl_easy_init();

    DlData_direct dlData_direct { std::ofstream(destination, std::ofstream::out | std::ofstream::app), until };

    //curl_easy_setopt(easyhandle, CURLOPT_URL, "");
    curl_easy_setopt(easyhandle, CURLOPT_URL, url.c_str());

    curl_easy_setopt(easyhandle, CURLOPT_WRITEFUNCTION, write_callback_direct);
    curl_easy_setopt(easyhandle, CURLOPT_WRITEDATA, &dlData_direct);

    CURLcode success = curl_easy_perform(easyhandle);

    curl_easy_cleanup(easyhandle);
}

size_t write_callback_m3u(char* ptr, size_t size, size_t nmemb, void *userdata){
    std::string* m3u = static_cast<std::string*>(userdata);

    m3u->append(ptr, size * nmemb);

    return size * nmemb;
}

void download_m3u(const std::string& url, const std::string& destination, boost::posix_time::ptime until){
    CURL* easyhandle = curl_easy_init();
    std::string m3u;

    curl_easy_setopt(easyhandle, CURLOPT_URL, url.c_str());

    curl_easy_setopt(easyhandle, CURLOPT_WRITEFUNCTION, write_callback_m3u);
    curl_easy_setopt(easyhandle, CURLOPT_WRITEDATA, &m3u);

    CURLcode success = curl_easy_perform(easyhandle);

    curl_easy_cleanup(easyhandle);

    std::cout << "'" << m3u << "'\n";

    size_t begin = 0; size_t end = m3u.find_first_of("\r\n", begin);
    while(boost::posix_time::microsec_clock::local_time() <= until && begin != std::string::npos){
        std::string snip = m3u.substr(begin, end != std::string::npos ? end-begin : std::string::npos);
        snip.erase(snip.find_last_not_of(" \n\r\t")+1);

        if(!snip.empty() && snip.front() != '#'){
            std::cout << "redirecting to -|" << snip << "|-\n";
            download_direct(snip, destination, until);
        }

        begin = end;
        end = m3u.find_first_of("\r\n", begin);
    }

}


class Programme {
public:
    std::string station;
    std::string name;
    std::shared_ptr<NextFunctor::Base> next;
    boost::posix_time::time_duration duration;

    Programme(const std::string& station, const std::string& name, const std::string& next, const boost::posix_time::time_duration& duration):
        station(station),
        name(name),
        next(NextFunctor::Base::parse(next)),
        duration(duration)
    {}
};

class Station {
public:
    std::string name;
    std::string url;

    Station(const std::string& name, const std::string& url):
        name(name),
        url(url)
    {}
};

class Event {
public:
    std::string station;
    std::string programme;
    boost::posix_time::ptime time;

    Event(const std::string& station, const std::string& programme, const boost::posix_time::ptime& time):
        station(station),
        programme(programme),
        time(time)
    {}

    friend bool operator<(const Event& ev1, const Event& ev2){
        return ev1.time > ev2.time; //reverse order in priority_queue
        //pop() shall return the lowest element
    }
};

class Scheduler {
    std::map<std::string, Station> stations;
    std::map<std::tuple<std::string, std::string>, Programme> programmes;
    std::priority_queue<Event> schedule;
public:
    Scheduler(const std::initializer_list<Station>& stations, const std::initializer_list<Programme>& programmes)
    {
        for(const auto& station: stations){
            assert(std::get<1>(this->stations.insert({station.name, station})));
        }

        for(const auto& programme: programmes){
            assert(std::get<1>(this->programmes.insert({{programme.station, programme.name}, programme})));
        }
    }

    void run(){
        for(auto& val: programmes){
            Programme& programme = std::get<1>(val);
            auto when = (*programme.next)(boost::posix_time::microsec_clock::local_time(), false);
            std::cout << "pushing event " << programme.station << "-" << programme.name << " " << when << "\n";
            schedule.push(Event(programme.station, programme.name, when));
        }

        while(!schedule.empty()){
            std::cout << "looping!\n";

            boost::posix_time::ptime now = boost::posix_time::microsec_clock::local_time();
            Event event = schedule.top();

            auto programme_it = programmes.find({event.station, event.programme});
            auto station_it = stations.find(event.station);

            assert(programme_it != programmes.end());
            assert(station_it != stations.end());

            auto& programme = programme_it->second;
            auto& station = station_it->second;

            auto diff = (event.time - now);
            if(diff.total_microseconds() > 0){
                std::cout << "sleeping for " << diff << " for " << event.station << "-" << event.programme << "\n";
                std::this_thread::sleep_for(std::chrono::microseconds(diff.total_microseconds()));
            }

            std::string prefixPath = "/tmp/feedme-core/";

            boost::filesystem::path dir(prefixPath);
            boost::filesystem::create_directory(prefixPath);

            std::thread thread(download_m3u, station.url, prefixPath + "/" + programme.station + "-" + programme.name + "-" + boost::posix_time::to_iso_extended_string(event.time), event.time + programme.duration);
            thread.detach();

            schedule.pop();

            auto when = (*programme.next)(event.time, true);
            schedule.push(Event(programme.station, programme.name, when));
        }
    }
};

int main(){
    CURLcode curl = curl_global_init(CURL_GLOBAL_ALL);

    Scheduler* scheduler;

    {
        using P = Programme;
        using S = Station;

        using boost::posix_time::minutes;

        scheduler = new Scheduler(
            {
                S("1live", "http://www.wdr.de/wdrlive/media/einslive.m3u"),

                S("br3", "http://streams.br.de/bayern3_2.m3u"),

                S("ndr2", "http://www.ndr.de/resources/metadaten/audio/m3u/ndr2.m3u"),

                S("wdr2", "http://www.wdr.de/wdrlive/media/wdr2.m3u"),
                S("wdr5", "http://www.wdr.de/wdrlive/media/wdr5.m3u")
            },

            {
                P("1live", "1minute", "0S", minutes(2)),

                P("br3", "1minute", "0S", minutes(2)),

                P("ndr2", "1minute", "0S", minutes(2)),

                P("wdr2", "1minute", "0S", minutes(2)),

                P("wdr5", "1minute", "0S", minutes(2)),

                P("wdr5", "U22", "(22:05 & [MON | TUE | WED | THU | FRI])", minutes(55)),
                P("wdr5", "BerichteVonHeute", "(23:30 & [MON | TUE | WED | THU | FRI])", minutes(30)),

                P("wdr5", "Krimi", "(17:05 & SAT)", minutes(55)),
                P("wdr5", "Dok5", "(11:05 & SUN)", minutes(55)),
                P("wdr5", "Hörspiel", "(17:05 & SUN)", minutes(55)),
                P("wdr5", "LeseLounge", "(20:05 & SUN)", minutes(55)),
                P("wdr5", "LiederLounge", "(21:05 & SUN)", minutes(55)),
                P("wdr5", "ZeitZeichen", "9:45", minutes(15)),
                P("wdr5", "EchoDesTages", "18:30", minutes(30)),
            }
        );
    }

    scheduler->run();

    /*
        http://www.wdr.de/wdrlive/media/wdr5.m3u
        http://wdr-5.akacast.akamaistream.net/7/41/119439/v1/gnl.akacast.akamaistream.net/wdr-5

        http://www.wdr.de/wdrlive/media/wdr2.m3u
        http://wdr-mp3-m-wdr2-koeln.akacast.akamaistream.net/7/812/119456/v1/gnl.akacast.akamaistream.net/wdr-mp3-m-wdr2-koeln
    */

    /*
    std::thread wdr5(download, "http://wdr-5.akacast.akamaistream.net/7/41/119439/v1/gnl.akacast.akamaistream.net/wdr-5", "/tmp/wdr5");
    std::thread wdr2(download, "http://wdr-mp3-m-wdr2-koeln.akacast.akamaistream.net/7/812/119456/v1/gnl.akacast.akamaistream.net/wdr-mp3-m-wdr2-koeln", "/tmp/wdr2");

    wdr5.join();
    wdr2.join();
    */
}