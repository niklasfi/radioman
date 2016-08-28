#include <curl/curl.h>
#include <libconfig.h++>

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
    const long timeout_m3u;
    const long timeout_direct;
private:
//owned and (predominantly) managed by the cURL thread
//make sure to acquire the mutex before accessing `sinks`
    std::unique_ptr<std::mutex> sinks_mutex;
    std::vector<Sink> sinks;
    boost::posix_time::ptime last_progress_time;
    curl_off_t last_progress_bytes;
//owned by the main/schedule management thread
    std::thread thread;

public:
    void spawn_direct(const std::string& url){
        this->thread = std::thread(&Station::download_direct_loop, this, url); //.detach();
    }

    void spawn_m3u(const std::string& url){
        this->thread = std::thread(&Station::download_m3u_loop, this, url); //.detach();
    }

    void attach(Sink&& sink){
        //called by the scheduling thread to notify the Station of a new Sink
        std::lock_guard<std::mutex> lock(*sinks_mutex);
        sinks.emplace_back(std::move(sink));
    }
    Station(size_t id, const std::string& name, const std::string& original_url, long timeout_m3u, long timeout_direct):
        id(id),
        name(name),
        original_url(original_url),
        timeout_m3u(timeout_m3u),
        timeout_direct(timeout_direct),
        sinks_mutex(new std::mutex()),
        sinks(),
        last_progress_time(boost::posix_time::not_a_date_time),
        last_progress_bytes(0),
        thread()
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

    static int progress_callback_direct(void* userdata, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow){
        (void) dltotal;
        (void) ultotal;
        (void) ulnow;

        Station* station = static_cast<Station*>(userdata);
        boost::posix_time::ptime now(boost::posix_time::microsec_clock::local_time());

        if(station->last_progress_time.is_not_a_date_time() || dlnow != station->last_progress_bytes){
            //std::cout << "dT: " << (now - station->last_progress_time) << std::endl;

            station->last_progress_time = now;
            station->last_progress_bytes = dlnow;
            return 0;
        }
        if(now - station->last_progress_time > boost::posix_time::seconds(station->timeout_direct)){
            std::cout << "[ERR] " << station->name << " info timeout" << std::endl;

            station->last_progress_time = boost::posix_time::not_a_date_time;
            station->last_progress_bytes = 0;
            return -1;
        }

        return 0;
    }

    void download_direct(const std::string& url){
        CURL* easyhandle = curl_easy_init();
        curl_easy_setopt(easyhandle, CURLOPT_URL, url.c_str());

        curl_easy_setopt(easyhandle, CURLOPT_WRITEFUNCTION, write_callback_direct);
        curl_easy_setopt(easyhandle, CURLOPT_WRITEDATA, this);

        curl_easy_setopt(easyhandle, CURLOPT_XFERINFOFUNCTION, progress_callback_direct);
        curl_easy_setopt(easyhandle, CURLOPT_XFERINFODATA, this);

        curl_easy_setopt(easyhandle, CURLOPT_NOPROGRESS, 0L);

        //std::cout << name << " direct download starts: " << url << std::endl;
        CURLcode success = curl_easy_perform(easyhandle);
        if (success != CURLE_OK && success != CURLE_WRITE_ERROR){
            std::cout << "[ERR] " << name << " " << curl_easy_strerror(success) << std::endl;
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

        curl_easy_setopt(easyhandle, CURLOPT_TIMEOUT, timeout_m3u);

        //std::cout << name << " m3u download starts" << std::endl;
        CURLcode success = curl_easy_perform(easyhandle);
        if (success != CURLE_OK && success != CURLE_WRITE_ERROR){
            std::cout << "[ERR] " << name << " " << curl_easy_strerror(success) << std::endl;
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
    Scheduler():
        destinationPath(),
        stations(),
        programmes(),
        schedule()
    {}

    int readConfig(const std::string& config_path){
        using namespace libconfig;

        Config cfg;

        try
        {
            cfg.readFile(config_path.c_str());
        }
        catch(const FileIOException &fioex)
        {
            std::cerr << "I/O error while reading file." << std::endl;
            return(EXIT_FAILURE);
        }
        catch(const ParseException &pex)
        {
            std::cerr << "Parse error at " << pex.getFile() << ":" << pex.getLine()
                    << " - " << pex.getError() << std::endl;
            return(EXIT_FAILURE);
        }

        try
        {
            destinationPath = static_cast<const char*>(cfg.lookup("destinationPath"));
        }
        catch(const SettingNotFoundException &nfex)
        {
            std::cerr << "No 'destinationPath' setting in configuration file." << std::endl;
            return(EXIT_FAILURE);
        }

        long timeout_m3u;
        long timeout_direct;

        try
        {
            timeout_m3u = cfg.lookup("timeoutM3U");
        }
        catch(const SettingNotFoundException &nfex)
        {
            std::cerr << "No 'timeoutM3U' setting in configuration file." << std::endl;
            return(EXIT_FAILURE);
        }

        try
        {
            timeout_direct = cfg.lookup("timeoutDirect");
        }
        catch(const SettingNotFoundException &nfex)
        {
            std::cerr << "No 'timeoutDirect' setting in configuration file." << std::endl;
            return(EXIT_FAILURE);
        }

        try
        {
            const Setting& schedule_setting = cfg.lookup("schedule");

            int station_count = schedule_setting.getLength();
            for(int i = 0; i < station_count; ++i){
                const Setting& station_setting = schedule_setting[i];

                std::string station_identifier;
                std::string station_url;

                try
                {
                    station_identifier = static_cast<const char*>(station_setting[0]);
                }
                catch(const SettingNotFoundException &nfex)
                {
                    std::cerr << "Station without identifier found" << std::endl;
                    return(EXIT_FAILURE);
                }

                try
                {
                    station_url = static_cast<const char*>(station_setting[1]);
                }
                catch(const SettingNotFoundException &nfex)
                {
                    std::cerr << "Station " << station_identifier << " has no URL" << std::endl;
                    return(EXIT_FAILURE);
                }
                stations.emplace_back(stations.size(), station_identifier, station_url, timeout_m3u, timeout_direct);

                try
                {
                    const Setting& programmes_setting = station_setting[2];

                    int programme_count = programmes_setting.getLength();
                    for(int j = 0; j < programme_count; ++j){
                        const Setting& programme_setting = programmes_setting[j];

                        std::string programme_identifier;
                        std::string programme_schedule;
                        int programme_duration;

                        try
                        {
                            programme_identifier = static_cast<const char*>(programme_setting[0]);
                        }
                        catch(const SettingNotFoundException &nfex)
                        {
                            std::cerr << "Programme without identifier found for station " << station_identifier << std::endl;
                            return(EXIT_FAILURE);
                        }

                        try
                        {
                            programme_schedule = static_cast<const char*>(programme_setting[1]);
                        }
                        catch(const SettingNotFoundException &nfex)
                        {
                            std::cerr << "Programme " << station_identifier << "-" << programme_identifier << " has no schedule string" << std::endl;
                            return(EXIT_FAILURE);
                        }

                        try
                        {
                            programme_duration = programme_setting[2];
                        }
                        catch(const SettingNotFoundException &nfex)
                        {
                            std::cerr << "Programme " << station_identifier << "-" << programme_identifier << " has no duration" << std::endl;
                            return(EXIT_FAILURE);
                        }

                        programmes.emplace_back(stations.size() - 1, programmes.size(), programme_identifier, NextFunctor::Base::parse(programme_schedule), boost::posix_time::minutes(programme_duration));
                    }
                }
                catch(const SettingNotFoundException &nfex)
                {
                    std::cerr << "Station " << station_identifier << " has no programmes" << std::endl;
                    return(EXIT_FAILURE);
                }
            }
        }
        catch(const SettingNotFoundException &nfex)
        {
            std::cerr << "No 'schedule' setting in configuration file." << std::endl;
            return(EXIT_FAILURE);
        }

        return EXIT_SUCCESS;
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
                //std::cout << "sleeping for " << diff << " for " << station.name << "-" << programme.name << std::endl;
                std::cout << event.time << " SLEEP " << station.name << "-" << programme.name << " for " << diff << std::endl;
                std::this_thread::sleep_for(std::chrono::microseconds(diff.total_microseconds()));
            }

            std::string prefixPath = destinationPath + "/" + station.name + "-" + programme.name;

            boost::filesystem::path dir(prefixPath);
            boost::filesystem::create_directories(prefixPath);

            std::string targetPath = prefixPath + "/" + station.name + "-" + programme.name + "-" + boost::posix_time::to_iso_extended_string(event.time) + ".mp3";
            std::unique_ptr<std::ofstream> ofs(new std::ofstream(targetPath, std::ofstream::out | std::ofstream::app));
            station.attach(Sink(event.time + programme.duration, std::move(ofs)));

            std::cout << event.time << " START " << station.name << "-" << programme.name << " for " << programme.duration << std::endl;

            schedule.pop();

            auto when = (*programme.next)(event.time, true);
            schedule.push(Event(event.programme, when));
        }
    }
};

int main(int argc, const char* argv[]){
    if(argc != 2){
        std::cout << "usage: " << argv[0] << " configuration_path" << std::endl;
        return -1;
    }

    Scheduler scheduler;

    if(scheduler.readConfig(argv[1]) != EXIT_SUCCESS){
        std::cout << "parsing configuration file " << argv[1] << " failed.\nexiting" << std::endl;
        return -1;
    }

    CURLcode curl = curl_global_init(CURL_GLOBAL_ALL);
    assert(curl == CURLE_OK);

    scheduler.run();
}