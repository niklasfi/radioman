#include <curl/curl.h>

#include <iostream>
#include <sstream>
#include <fstream>

#include <mutex>
#include <thread>
#include <vector>
#include <queue>

#include "next.h"

#include <boost/filesystem.hpp>

class ConfigurationProgramme {
public:
    std::string station;
    std::string name;
    std::shared_ptr<NextFunctor::Base> next;
    boost::posix_time::time_duration duration;

    ConfigurationProgramme(const std::string& station, const std::string& name, const std::string& next, const boost::posix_time::time_duration& duration):
        station(station),
        name(name),
        next(NextFunctor::Base::parse(next)),
        duration(duration)
    {}
};

class Programme {
public:
    const size_t station_id;
    const size_t programme_id;
    const std::string name;
    std::shared_ptr<NextFunctor::Base> next;
    const boost::posix_time::time_duration duration;

    Programme(const size_t station_id, const size_t programme_id, const std::string& name, std::shared_ptr<NextFunctor::Base> next, const boost::posix_time::time_duration duration):
        station_id(station_id),
        programme_id(programme_id),
        name(name),
        next(next),
        duration(duration)
    {}
};

class ConfigurationStation {
public:
    std::string name;
    std::string url;

    ConfigurationStation(const std::string& name, const std::string& url):
        name(name),
        url(url)
    {}
};

class Sink{
    boost::posix_time::ptime valid_until;
    std::unique_ptr<std::ofstream> destination;
public:
    Sink(const boost::posix_time::ptime& valid_until, std::unique_ptr<std::ofstream>&& destination):
        valid_until(valid_until),
        destination(std::move(destination))
    {}
    Sink(Sink&& sink):
        valid_until(sink.valid_until),
        destination(std::move(sink.destination))
    {}
    Sink& operator=(Sink&& sink) = default;
    Sink operator=(const Sink& sink) = delete;
    friend class Station;
};

class Station {
public:
//const and therefore freely accessible
    const size_t id;
    const std::string name;
    const std::string original_url;

private:
//owned and (predominantly) managed by the cURL thread
//make sure to acquire the mutex before accessing `sinks`
    std::unique_ptr<std::mutex> sinks_mutex;
    std::vector<Sink> sinks;

public:
    void spawn_direct(const std::string& url){
        std::thread(&Station::download_direct_loop, this, url).detach();
    }

    void spawn_m3u(const std::string& url){
        std::thread(&Station::download_m3u_loop, this, url).detach();
    }

    void attach(Sink&& sink){
        //called by the scheduling thread to notify the Station of a new Sink
        std::lock_guard<std::mutex> lock(*sinks_mutex);
        sinks.emplace_back(std::move(sink));
    }
    Station(size_t id, const std::string& name, const std::string& original_url):
        id(id),
        name(name),
        original_url(original_url),
        sinks_mutex(new std::mutex()),
        sinks()
    {}
private:
    static size_t write_callback_direct(char *ptr, size_t size, size_t nmemb, void *userdata){
        Station* station = static_cast<Station*>(userdata);

        std::lock_guard<std::mutex> lock(*station->sinks_mutex);

        //erase expired sinks
        boost::posix_time::ptime now(boost::posix_time::microsec_clock::local_time());
        station->sinks.erase(std::remove_if(station->sinks.begin(), station->sinks.end(), [&now](const Sink& sink){return sink.valid_until < now;}), station->sinks.end());

        //write received data to all of the sinks
        for(auto& sink: station->sinks){
            sink.destination->write(ptr, size * nmemb);
        }

        return size * nmemb;
    }

    static size_t write_callback_m3u(char* ptr, size_t size, size_t nmemb, void *userdata){
        std::string* m3u = static_cast<std::string*>(userdata);

        m3u->append(ptr, size * nmemb);

        return size * nmemb;
    }

    void download_direct(const std::string& url){
        CURL* easyhandle = curl_easy_init();
        curl_easy_setopt(easyhandle, CURLOPT_URL, url.c_str());

        curl_easy_setopt(easyhandle, CURLOPT_WRITEFUNCTION, write_callback_direct);
        curl_easy_setopt(easyhandle, CURLOPT_WRITEDATA, this);

        //std::cout << name << " direct download starts\n";
        CURLcode success = curl_easy_perform(easyhandle);
        if (success != CURLE_OK && success != CURLE_WRITE_ERROR){
            std::cout << "[ERR] " << name << " " << curl_easy_strerror(success) << "\n";
        }

        curl_easy_cleanup(easyhandle);
    }

    void download_direct_loop(const std::string& url){
        while(true){
            download_direct(url);
        }
    }

    void download_m3u(const std::string& url){
        CURL* easyhandle = curl_easy_init();
        std::string m3u;

        curl_easy_setopt(easyhandle, CURLOPT_URL, url.c_str());

        curl_easy_setopt(easyhandle, CURLOPT_WRITEFUNCTION, write_callback_m3u);
        curl_easy_setopt(easyhandle, CURLOPT_WRITEDATA, &m3u);

        //std::cout << name << " m3u download starts\n";
        CURLcode success = curl_easy_perform(easyhandle);
        if (success != CURLE_OK && success != CURLE_WRITE_ERROR){
            std::cout << "[ERR] " << name << " " << curl_easy_strerror(success) << "\n";
        }

        curl_easy_cleanup(easyhandle);

        size_t begin = 0; size_t end = m3u.find_first_of("\r\n", begin);
        while(begin != std::string::npos){
            std::string snip = m3u.substr(begin, end != std::string::npos ? end-begin : std::string::npos);
            snip.erase(snip.find_last_not_of(" \n\r\t")+1);

            if(!snip.empty() && snip.front() != '#'){
                download_direct(snip);
            }

            begin = end;
            end = m3u.find_first_of("\r\n", begin);
        }
    }

    void download_m3u_loop(const std::string& url){
        while(true){
            download_m3u(url);
        }
    }
};

class Event {
public:
    size_t programme;
    boost::posix_time::ptime time;

    Event(size_t programme, const boost::posix_time::ptime& time):
        programme(programme),
        time(time)
    {}

    friend bool operator<(const Event& ev1, const Event& ev2){
        return ev1.time > ev2.time; //reverse order in priority_queue
        //pop() shall return the lowest element
    }
};

class Scheduler {
    std::string destinationPath;
    std::vector<Station> stations;
    std::vector<Programme> programmes;

    std::priority_queue<Event> schedule;


public:
    Scheduler(const std::string& destinationPath, const std::initializer_list<ConfigurationStation>& cStations, const std::initializer_list<ConfigurationProgramme>& cProgrammes):
        destinationPath(destinationPath)
    {
        for(const auto& cStation: cStations){
            stations.emplace_back(stations.size(), cStation.name, cStation.url);
        }

        for(const auto& cProgramme: cProgrammes){
            auto station_it = std::find_if(stations.begin(), stations.end(), [&cProgramme](const Station& station){return station.name == cProgramme.station;});
            assert(station_it != stations.end());

            programmes.emplace_back(std::distance(stations.begin(), station_it), programmes.size(), cProgramme.name, cProgramme.next, cProgramme.duration);
        }
    }

    void run(){
        //start the station curl threads
        for(auto& station: stations){
            station.spawn_m3u(station.original_url);
        }

        {
            boost::posix_time::ptime now(boost::posix_time::microsec_clock::local_time());
            for(auto& programme: programmes){
                auto when = (*programme.next)(now, false);
                schedule.push(Event(programme.programme_id, when));
            }
        }

        while(!schedule.empty()){
            Event event = schedule.top();

            Programme& programme(programmes.at(event.programme));
            Station& station(stations.at(programme.station_id));

            boost::posix_time::ptime now = boost::posix_time::microsec_clock::local_time();
            auto diff = (event.time - now);
            if(diff.total_microseconds() > 0){
                std::cout << "sleeping for " << diff << " for " << station.name << "-" << programme.name << "\n";
                std::this_thread::sleep_for(std::chrono::microseconds(diff.total_microseconds()));
            }

            std::string prefixPath = destinationPath + "/" + station.name + "-" + programme.name;

            boost::filesystem::path dir(prefixPath);
            boost::filesystem::create_directory(prefixPath);

            std::string targetPath = prefixPath + "/" + station.name + "-" + programme.name + "-" + boost::posix_time::to_iso_extended_string(event.time) + ".mp3";
            std::unique_ptr<std::ofstream> ofs(new std::ofstream(targetPath, std::ofstream::out | std::ofstream::app));
            station.attach(Sink(event.time + programme.duration, std::move(ofs)));

            schedule.pop();

            auto when = (*programme.next)(event.time, true);
            schedule.push(Event(event.programme, when));
        }
    }
};

int main(){
    CURLcode curl = curl_global_init(CURL_GLOBAL_ALL);
    assert(curl == CURLE_OK);

    Scheduler* scheduler;

    {
        using P = ConfigurationProgramme;
        using S = ConfigurationStation;

        using boost::posix_time::minutes;

        scheduler = new Scheduler( "/var/www/feedme-core/media/",
            {
                S("1live", "http://www.wdr.de/wdrlive/media/einslive.m3u"),

                S("br3", "http://streams.br.de/bayern3_2.m3u"),

                S("ndr2", "http://www.ndr.de/resources/metadaten/audio/m3u/ndr2.m3u"),

                S("wdr2", "http://www.wdr.de/wdrlive/media/wdr2.m3u"),
                S("wdr3", "http://www.wdr.de/wdrlive/media/wdr3.m3u"),
                S("wdr5", "http://www.wdr.de/wdrlive/media/wdr5.m3u")
            },

            {
                P("1live", "Krimi-Shortstory", "(23:05 & THU)", minutes(55)),
                P("1live", "Domian", "(01:05 & [TUE | WED | THU | FRI | SAT])", minutes(55)),
                P("1live", "DJ_Session", "(00:00 & SUN)", minutes(180)),
                P("1live", "Kassettendeck", "(23:05 & MON)", minutes(55)),

                //P("br3", "1minute", "0S", minutes(2)),

                //P("ndr2", "1minute", "0S", minutes(2)),

                //P("wdr2", "1minute", "0S", minutes(2)),

                P("wdr3", "Hoerrspiel", "(19:04 & [MON | TUE | WED | THU | FRI])", minutes(56)),
                P("wdr3", "ARD_Radiofestival._Konzert", "(20:04 & [MON | TUE | WED | THU | FRI | SUN])", minutes(146)),
                P("wdr3", "ARD_Radiofestival._Kabarett", "(22:30 & SUN)", minutes(60)),
                P("wdr3", "ARD_Radiofestival._Lesung", "(22:30 & [MON | TUE | WED | THU | FRI])", minutes(30)),

                //P("wdr5", "1minute", "0S", minutes(2)),

                P("wdr5", "Profit", "(18:05 & [MON | TUE | WED | THU | FRI | SAT])", minutes(25)),
                P("wdr5", "Echo_des_Tages", "18:30", minutes(30)),
                P("wdr5", "Europamagazin", "(20:05 & TUE)", minutes(55)),
                P("wdr5", "Tischgespraech", "(20:05 & WED)", minutes(55)),
                P("wdr5", "Funkhausgespraeche", "(20:05 & THU)", minutes(55)),
                P("wdr5", "Das_philosophische Radio", "(20:05 & FRI)", minutes(55)),
                P("wdr5", "U22", "(22:05 & [MON | TUE | WED | THU | FRI])", minutes(55)),
                P("wdr5", "Berichte_von_heute", "(23:30 & [MON | TUE | WED | THU | FRI])", minutes(30)),
                P("wdr5", "Krimi", "(17:05 & SAT)", minutes(55)),
                P("wdr5", "Dok5", "(11:05 & SUN)", minutes(55)),
                P("wdr5", "Presseclub", "(12:00 & SUN)", minutes(60)),
                P("wdr5", "Hoerspiel", "(17:05 & SUN)", minutes(55)),
                P("wdr5", "LeseLounge", "(20:05 & SUN)", minutes(55)),
                P("wdr5", "LiederLounge", "(21:05 & SUN)", minutes(55)),
                P("wdr5", "ZeitZeichen", "9:45", minutes(15)),
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