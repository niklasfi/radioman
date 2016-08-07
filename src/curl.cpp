#include "curl.h"

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/asio.hpp>

#include <curl/curl.h>

#include <memory>
#include <fstream>
#include <functional>
#include <string>
#include <vector>

struct FileWriter{
    std::string path;
    std::ofstream ofs;
    
    FileWriter(const std::string& path): path(path), ofs(path){}
    
    ~FileWriter(){
        
    }
    
    static size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp){
        FileWriter* fw = reinterpret_cast<FileWriter*>(userp);
        
        fw->ofs.write((char*)contents, size * nmemb);
        
        return size * nmemb;
    }
};

struct Listener{
    boost::posix_time::ptime until;
    std::ofstream ofs;
    
    Listener(boost::posix_time::ptime until, const std::string& path): until(until), ofs(path){}
    Listener(Listener&& l) = default;
    Listener(const Listener&) = delete;
    Listener& operator=(const Listener&) = delete;
    Listener& operator=(Listener&&) throw() {return *this; }
    
    bool is_valid(boost::posix_time::ptime when) const{
        return when <= until;
    }
    
    void write(char* contents, size_t nbytes){
        ofs.write(contents, nbytes);
    }
};

struct Stream{
    std::string url;
    CURL* curl_handle;
    char error[CURL_ERROR_SIZE];
    
    std::vector<Listener> listeners;
    
    Stream(const std::string& url): url(url), curl_handle(curl_easy_init()){
        curl_easy_setopt(curl_handle, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, &Stream::writeCallback);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, this);
    }
    
    ~Stream(){
        curl_easy_cleanup(curl_handle);
    }
    
    void createListener(boost::posix_time::ptime until, std::string path){
        listeners.emplace_back(Listener(until, path));
    }
    
    static size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp){
        Stream* s = reinterpret_cast<Stream*>(userp);
        
        boost::posix_time::ptime now(boost::posix_time::microsec_clock::local_time());
        for(auto it = s->listeners.begin(); it != s->listeners.end(); ){
            if(it->is_valid(now)){
                it->write(reinterpret_cast<char*>(contents), size * nmemb);
                ++it;
            }
            else{
                it = s->listeners.erase(it);
            }
        }
        
        return size * nmemb;
    }
    
    void perform(){
        curl_easy_perform(curl_handle);
    }
};

/*
struct StreamResponse {
protected:
    StreamResponse() = default;
public:
    virtual ~StreamResponse() = default;
    virtual void data_function(char* data, long size) = 0;
    virtual void signal_function(int signal) = 0;
};

struct OStreamStreamResponse : public StreamResponse {
    std::ostream& os;
    OStreamStreamResponse(std::ostream&& os): os(os){}
    virtual void data_function(char* data, long size){
        os.write(data, size);
    }
    virtual void signal_function(int signal){
        
    }
};
*/

struct CurlMultiWrapper{
    using socket_ptr = boost::asio::ip::tcp::socket*;  
    using socket_map_t = std::map<curl_socket_t, socket_ptr>;
    using curl_easy_handle_map_t = std::map<CURL* , socket_map_t>;
    
    CURLM* curl_multi_handle;
    boost::asio::io_service* io_service;
    boost::asio::deadline_timer timer;
    curl_easy_handle_map_t curl_easy_handle_map;

    CurlMultiWrapper(boost::asio::io_service* io_service):curl_multi_handle(nullptr), io_service(io_service), timer(*io_service){
        this->curl_multi_handle = curl_multi_init();
        
        {
            using namespace std::placeholders;
            curl_multi_setopt(this->curl_multi_handle, CURLMOPT_SOCKETFUNCTION, &curlmopt_socketfunction);
            curl_multi_setopt(this->curl_multi_handle, CURLMOPT_SOCKETDATA, reinterpret_cast<char*>(this));
            curl_multi_setopt(this->curl_multi_handle, CURLMOPT_TIMERFUNCTION, &curlmopt_timerfunction);
            curl_multi_setopt(this->curl_multi_handle, CURLMOPT_TIMERDATA, reinterpret_cast<char*>(this));
        }
    }
    
    ~CurlMultiWrapper(){
        for(auto item: curl_easy_handle_map){
            this->remove(item.first);
        }        
        
        curl_multi_cleanup(this->curl_multi_handle);
    }
    
    void add(CURL* curl_easy_handle){
        curl_easy_handle_map_t::iterator it;
        bool insert_successful;
        std::tie(it, insert_successful) = curl_easy_handle_map.emplace(curl_easy_handle, socket_map_t());
        
        assert(insert_successful);
        
        curl_multi_add_handle(curl_multi_handle, curl_easy_handle);
    }
    
    void remove(CURL* curl_easy_handle){
        curl_easy_handle_map_t::iterator handle_it = curl_easy_handle_map.find(curl_easy_handle);
        assert(handle_it != curl_easy_handle_map.end());
        
        for(auto socket_item: handle_it->second){
            delete socket_item.second;
        }
        
        handle_it->second.clear();
        
        curl_multi_remove_handle(curl_multi_handle, curl_easy_handle);
    }

    //called the asio timer upon timeout
    static void asio_timer_callback(CurlMultiWrapper* curl_multi_wrapper, const boost::system::error_code& error){
        
        //call curl_multi_socket_action here
        if(!error)
        {
            asio_generalized_callback(curl_multi_wrapper, CURL_SOCKET_TIMEOUT, 0);
        }
        
        //check multi info
    }
    
    static void asio_socket_callback(CurlMultiWrapper* curl_multi_wrapper, curl_socket_t socket_id, int what){
        asio_generalized_callback(curl_multi_wrapper, socket_id, what);
    }
    
    static void asio_generalized_callback(CurlMultiWrapper* curl_multi_wrapper, curl_socket_t socket_id, int what){
        int remaining_easy_handles;
        CURLMcode rc = curl_multi_socket_action(curl_multi_wrapper->curl_multi_handle, socket_id, what, &remaining_easy_handles);
        //check multi info
    }
    
    static int curlmopt_timerfunction(CURLM* curl_multi_handle, long timeout_ms, void* curlmopt_timerdata){
        CurlMultiWrapper& curl_multi_wrapper = *reinterpret_cast<CurlMultiWrapper*>(curlmopt_timerdata);
        
        curl_multi_wrapper.timer.cancel();
        
        if(timeout_ms > 0){
            curl_multi_wrapper.timer.expires_from_now(boost::posix_time::millisec(timeout_ms));
            curl_multi_wrapper.timer.async_wait(std::bind(&asio_timer_callback, &curl_multi_wrapper, std::placeholders::_1));
        }
        
        //return 0 on success, and -1 on error
        return 0;
    }
    
    static int curlmopt_socketfunction(CURL* curl_easy_handle, curl_socket_t socket_id, int what, void* curlmopt_socketdata, void* curl_multi_assign_sockptr){
        CurlMultiWrapper& curl_multi_wrapper = *reinterpret_cast<CurlMultiWrapper*>(curlmopt_socketdata);
        
        curl_easy_handle_map_t::iterator handle_it = curl_multi_wrapper.curl_easy_handle_map.end();
        assert(handle_it != curl_multi_wrapper.curl_easy_handle_map.end());
        
        socket_map_t::iterator socket_it = handle_it->second.find(socket_id);
        
        if(socket_it == handle_it->second.end()){
            bool insert_successful;
            boost::asio::ip::tcp::socket* tcp_socket = new boost::asio::ip::tcp::socket(*curl_multi_wrapper.io_service);
            
            tcp_socket->assign(boost::asio::ip::tcp::v4(), socket_id);
            
            std::tie(socket_it, insert_successful) = handle_it->second.emplace(socket_id, std::move(tcp_socket));
            assert(insert_successful);
        }
        
        boost::asio::ip::tcp::socket* tcp_socket = socket_it->second;
        
        if(what == CURL_POLL_IN){
            //waiting for which_socket to become readable
            tcp_socket->async_read_some(boost::asio::null_buffers(), std::bind(&asio_socket_callback, &curl_multi_wrapper, socket_id, what));
        }
        else if(what == CURL_POLL_OUT){
            //waiting for which_socket to become writable
            tcp_socket->async_write_some(boost::asio::null_buffers(), std::bind(&asio_socket_callback, &curl_multi_wrapper, socket_id, what));
        }
        else if(what == CURL_POLL_INOUT){
            //waiting for which_socket to become read or writable
            tcp_socket->async_read_some(boost::asio::null_buffers(), std::bind(&asio_socket_callback, &curl_multi_wrapper, socket_id, what));
            tcp_socket->async_write_some(boost::asio::null_buffers(), std::bind(&asio_socket_callback, &curl_multi_wrapper, socket_id, what));
        }
        else if(what == CURL_POLL_REMOVE){
            //remove socket
            delete socket_it->second;
            handle_it->second.erase(socket_it->first);
        }
        else{
            assert(false);
        }
        
        //this function has to return 0
        return 0;
    }
};

int main(){
    curl_global_init(CURL_GLOBAL_ALL);
    
    Stream s("http://1live.akacast.akamaistream.net/7/706/119434/v1/gnl.akacast.akamaistream.net/1live");
    s.createListener(boost::posix_time::microsec_clock::local_time() + boost::posix_time::seconds(10), "file.mp3");
    
    s.perform();
    
    curl_global_cleanup();
    
    /*
    CURL* curl_handle(curl_easy_init());
    
    curl_easy_setopt(curl_handle, CURLOPT_URL, "http://1live.akacast.akamaistream.net/7/706/119434/v1/gnl.akacast.akamaistream.net/1live");
    
    FileWriter fw("file.mp3");
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, &FileWriter::writeCallback);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void*)&fw);
    
    res = curl_easy_perform(curl_handle);   
    */
}