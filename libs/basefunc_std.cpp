#include "basefunc_std.h"


std::mutex basefunc_std::mutex_log;
std::string basefunc_std::db_path_log = "/var/log/_LOG/";

std::thread basefunc_std::thread_log_flush;
std::atomic<bool> basefunc_std::is_log_thread_started;

std::map<std::string, std::vector<std::string> > basefunc_std::map_log;
 std::vector<std::string> basefunc_std::created_patheth;

basefunc_std::basefunc_std()
{

}

/**
 * EXEC CMD AND RETURN OUTPUT
 */
std::string basefunc_std::execCMD(const std::string& cmd)
{
    FILE* pipe = popen(cmd.c_str(), "r");

    if (!pipe) return "";
    char buffer[128];
    std::string result;
    while(!feof(pipe))
    {
        if(fgets(buffer, 128, pipe) != NULL)
        result += buffer;
    }

    pclose(pipe);
    return result;
}

/**
 * EXPLODE SKIP EMPTY (slowest version)
 */
std::vector<std::string> basefunc_std::split_regex(const std::string &s, const std::string& delim) noexcept
{
    std::vector<std::string> elems;

    std::regex r(delim);
    std::sregex_token_iterator iter(s.begin(), s.end(), r, -1);
    std::sregex_token_iterator end;
    while (iter != end)
    {
        elems.push_back(*iter);
        ++iter;
    }

    return elems;
}

std::ostream& basefunc_std::cout(const std::string& text, const std::string& header, COLOR col, bool flush) noexcept
{
#ifndef NO_BASEFUCN_STD_COUT
    if (header.compare("") != 0)
    {
        switch (col)
        {
        case COLOR::GREEN_COL:
            std::cout << GREEN;
            break;
        case COLOR::RED_COL:
            std::cout << RED;
            break;
        case COLOR::YELLOW_COL:
            std::cout << YELLOW;
            break;
        case COLOR::CYAN_COL:
            std::cout << CYAN;
            break;
        case COLOR::BLUE_COL:
            std::cout << BLUE;
            break;
        case COLOR::MAGENTA_COL:
            std::cout << MAGENTA;
            break;
        default:
            break;
        }

        std::cout << "[" << header << "] " << RESET;
    }

    std::cout << text;
    if (flush) return std::cout << std::endl << std::flush;
    else return std::cout << " ";
#else
    return std::cout;
#endif

}

/**
 * СУЩЕСТВЕТ ЛИ ФАЙЛ
 */
bool basefunc_std::fileExists(const std::string& name)
{
    if (FILE *file = fopen(name.c_str(), "r"))
    {
        fclose(file);
        return true;
    }
    else
    {
        return false;
    }
}

std::vector<std::string> basefunc_std::read_files_in_folder(const std::string& folder)
{
    std::vector<std::string> ret;
    DIR *dpdf = opendir(folder.c_str());

    if (dpdf != NULL)
    {
        struct dirent *epdf;
        while (epdf = readdir(dpdf))
        {
            if (strcmp(epdf->d_name, ".") == 0 || strcmp(epdf->d_name, "..") == 0) continue;
            ret.push_back(epdf->d_name);
        }

        closedir(dpdf);
    }

    return ret;
}

void basefunc_std::chmod_recursive(const std::string& fileName, const std::string& mode)
{
    std::vector<std::string> exp = basefunc_std::split(fileName, '/');
    unsigned sz = exp.size();
    std::string addedPath = "/";

    for (unsigned i = 0; i < sz; ++i)
    {
        addedPath += exp.at(i);
        std::string path_dir = addedPath;

        chmod(path_dir.c_str(), strtol(mode.c_str(), 0, 8));

        addedPath += "/";
    }
}

void basefunc_std::create_folder_recursive(const std::string& fileName, unsigned start_from, bool create_last)
{
    std::vector<std::string> exp = basefunc_std::split(fileName, '/');
    unsigned sz = exp.size();
    std::string addedPath = "/";

    for (unsigned i = start_from; i < sz; ++i)
    {
        if (!create_last && i == sz - 1) break;

        addedPath += exp.at(i);
        std::string path_dir = addedPath;

        createFolder(path_dir.c_str());

        addedPath += "/";
    }
}

/**
 * СОЗДАТЬ ПАПКУ
 */
void basefunc_std::createFolder(const char *path)
{
    //Making DIR if not exists for log files
    DIR *pDir;
    pDir = opendir (path);

    if (pDir == NULL)
    {
#ifdef __WIN32__
        mkdir(path);
#else
        mkdir(path, 0777);
#endif
    }
    else (void) closedir (pDir);
}

void basefunc_std::check_system(bool exit_)
{
    if (sizeof(int) != sizeof(std::int32_t))
    {
        basefunc_std::log("size of int " + std::to_string(sizeof(int)) + " required 4 bytes", "system", true, basefunc_std::COLOR::RED_COL);
        if (exit_) exit(0);
    }
    if (sizeof(long) != sizeof(std::int64_t))
    {
        basefunc_std::log("size of long " + std::to_string(sizeof(long)) + " required 8 bytes", "system", true, basefunc_std::COLOR::RED_COL);
        if (exit_) exit(0);
    }


    struct rlimit lim;
    int r = getrlimit(RLIMIT_NOFILE, &lim);
    if (r == -1)
    {
        basefunc_std::log("NOFILE cant read errno: " + std::to_string(errno), "system", true, basefunc_std::COLOR::RED_COL);
    }
    else
    {
        #ifndef NDEBUGM
        constexpr auto min = 40000U;
        if (lim.rlim_cur < min)
        {
            basefunc_std::log("NOFILE rlimit too low: " + std::to_string(lim.rlim_cur) + ", minimum: " + std::to_string(min), "system", true, basefunc_std::COLOR::RED_COL);
            if (exit_) exit(0);
        }
        #endif
    }

    try
    {
        std::ifstream t("/proc/sys/net/ipv4/tcp_syncookies");
        std::string str((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());

        if (str.compare("0\n") == 0)
        {
            basefunc_std::cout("sysctl entry net.ipv4.tcp_syncookies is set to 0.\n"
                               "For better performance, set following parameter on sysctl is strongly recommended:\n"
                               "net.ipv4.tcp_syncookies=1", "system", basefunc_std::COLOR::RED_COL);
        }
    }
    catch (const std::system_error& e)
    {
        basefunc_std::cout("Unable to check if net.ipv4.tcp_syncookies is set {}", "system", basefunc_std::COLOR::RED_COL);
    }
}

std::string basefunc_std::get_path_log() noexcept
{
    return db_path_log;
}

void basefunc_std::set_path_log(const std::string& p, bool start_thread_log_flush) noexcept
{
    db_path_log = p;
    createFolder(db_path_log.c_str());

    if (!is_log_thread_started && start_thread_log_flush) //ЗАПУСК ПОТОКА FLUSH
    {
        is_log_thread_started = true;
        thread_log_flush = std::thread([]
        {
            int cnt = 0;
            while(is_log_thread_started)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                ++cnt;

                if (cnt >= 3000)
                {
                    log_flush();
                    cnt = 0;
                }
            }
            log_flush();
        });
        thread_log_flush.detach();
    }
}

void basefunc_std::stop_log_thread()
{
    if (is_log_thread_started)
    {
        is_log_thread_started = false;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

/**
 * ФЛУШ ЛОГА НА ДИСК
 */
void basefunc_std::log_flush()
{
    std::lock_guard<std::mutex> lock(mutex_log);

    for (auto it = map_log.begin(); it != map_log.end(); ++it)
    {
#ifndef basefunc_std_no_fstream
        try
        {
            std::fstream file(it->first, std::ios::out | std::ios::app); // | std::ios::binary
            if (!file) continue;
            for (auto& it2 : it->second)
            {
                file << it2;
            }
            file.close();
        }
        catch(...) { }
#else

        std::string txt_to_write;
        for (auto& it2 : it->second)
        {
            txt_to_write += it2;
        }

        FILE *file = fopen(it->first.c_str(), "ab");
        if (file)
        {
            fputs (txt_to_write.c_str(), file);
            fclose(file);
        }
#endif
    }

    map_log.clear();
}

/**
 * LOG
 */
void basefunc_std::log(const std::string& txt, std::string fileName, bool flush_im, COLOR col) noexcept
{
    if (col != COLOR::NONE)
    {
        basefunc_std::cout(txt, fileName, col);
    }

    time_t t = time(nullptr);
    //struct tm* now = localtime(&t);
    struct tm* now = new tm;
    localtime_r(&t, now);

    std::string date_now = std::to_string(now->tm_year + 1900) + "-";
    if (now->tm_mon + 1 < 10) date_now += "0";
    date_now += std::to_string(now->tm_mon + 1);
    std::string date_ym = date_now;
    date_now += "-";
    if (now->tm_mday < 10) date_now += "0";
    date_now += std::to_string(now->tm_mday);
    std::string time_now = std::to_string(now->tm_hour);

    std::string time_now_full = std::to_string(now->tm_hour) + ":";
    if (now->tm_hour < 10) time_now_full = "0" + time_now_full;
    if (now->tm_min < 10) time_now_full += "0";
    time_now_full += std::to_string(now->tm_min) + ":";
    if (now->tm_sec < 10) time_now_full += "0";
    time_now_full += std::to_string(now->tm_sec);

    delete now;

    basefunc_std::replaceAll(fileName, "$date_ym", date_ym);
    basefunc_std::replaceAll(fileName, "$date", date_now);
    basefunc_std::replaceAll(fileName, "$time", time_now);

    std::string txt_to_write = txt + " " + date_now + " " + time_now_full + "\r\n";
    std::string full_filename = basefunc_std::db_path_log + fileName;

    std::lock_guard<std::mutex> lock(mutex_log);

    //СОЗДАЕМ ПАПКИ
    if (std::find(created_patheth.begin(), created_patheth.end(), fileName) == created_patheth.end())
    {
        std::vector<std::string> exp = basefunc_std::split(fileName, '/');
        int sz = exp.size();
        std::string addedPath = "";

        for (int i = 0; i < sz - 1; ++i)
        {
            addedPath += exp.at(i);
            std::string path_dir = basefunc_std::db_path_log + addedPath;

            createFolder(path_dir.c_str());

            addedPath += "/";
        }

        created_patheth.push_back(fileName);
    }

    if (flush_im) //ЗАПИСЬ НЕМЕДЛЕННО
    {
#ifndef basefunc_std_no_fstream
        try
        {
            std::fstream file(full_filename, std::ios::out | std::ios::app); // | std::ios::binary
            if (file)
            {
                file << txt_to_write;
                file.close();
            }
        }
        catch(...) { }
#else
        FILE *file = fopen(full_filename.c_str(), "ab");
        if (file)
        {
            fputs (txt_to_write.c_str(), file);
            fclose(file);
        }
#endif
    }
    else //ЗАПИСЬ ВСЕ СРАЗУ
    {
        map_log[full_filename].push_back(txt_to_write);
    }
}

/**
 * LOG
 */
void basefunc_std::log(const std::string& txt, std::string header, std::string fileName, bool flush_im, COLOR col) noexcept
{
    basefunc_std::cout(txt, header, col, flush_im);
    basefunc_std::log(txt, fileName, flush_im, COLOR::NONE);
}

std::string basefunc_std::generate_pass(int len, bool use_lower_case)
{
    auto get_rand = [](int f, int t) -> int
    {
        while (true)
        {
            int r = rand(f, t);
            if (r != 111 && r != 79) return r;
        }
    };

    std::string pass;
    for (int i = 0; i < len; ++i)
    {
        if (i % 3 == 0)
        {
            if (use_lower_case) pass += char(get_rand(97, 122));
            else pass += char(get_rand(65, 90));
        }
        else if (i % 3 == 1) pass += char(get_rand(49, 57));
        else pass += char(get_rand(65, 90));
    }
    return pass;
}

std::string basefunc_std::generate_pass_wb(int len)
{
    auto get_rand = [](int f, int t) -> int
    {
        while (true)
        {
            int r = rand(f, t);
            if (r != 111 && r != 79) return r;
        }
    };

    const std::int32_t nc { 4 };

    std::set<std::int32_t> nn;
    std::string pass;
    for (int i = 0; i < len; ++i)
    {
        std::int32_t n { basefunc_std::rand(0, 100) % nc };

        if (i >= len - nc && nn.size() < nc)
        {
            for (auto j = 0; j < nc; ++j)
            {
                if (nn.count(j) == 0)
                {
                    n = j;
                    break;
                }
            }
        }

        nn.emplace(n);

        if (n == 0) pass += char(get_rand(97, 122));
        else if (n == 1) pass += char(get_rand(49, 57));
        else if (n == 2) pass += char(get_rand(65, 90));
        else pass += char(get_rand(33, 42));
    }

    return pass;
}

std::string basefunc_std::generate_uuid_v4()
{
    static std::random_device              rd;
    static std::mt19937                    gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static std::uniform_int_distribution<> dis2(8, 11);

    std::stringstream ss;
    int i;
    ss << std::hex;
    for (i = 0; i < 8; i++) {
        ss << dis(gen);
    }
    ss << "-";
    for (i = 0; i < 4; i++) {
        ss << dis(gen);
    }
    ss << "-4";
    for (i = 0; i < 3; i++) {
        ss << dis(gen);
    }
    ss << "-";
    ss << dis2(gen);
    for (i = 0; i < 3; i++) {
        ss << dis(gen);
    }
    ss << "-";
    for (i = 0; i < 12; i++) {
        ss << dis(gen);
    };
    return ss.str();
}

/**
 * REPLACE ALL
 */
void basefunc_std::replaceAll(std::string &s, const std::string &search, const std::string &replace) noexcept
{
    for (size_t pos = 0; ; pos += replace.length())
    {
        pos = s.find(search, pos);
        if (pos == std::string::npos) break;

        s.erase(pos, search.length());
        s.insert(pos, replace);
    }
}

void basefunc_std::replaceAll(std::string &s, const char &search, const char &replace) noexcept
{
    for (size_t pos = 0; ; pos += 1)
    {
        pos = s.find(search, pos);
        if (pos == std::string::npos) break;

        s.erase(pos, 1);
        s.insert(pos, &replace);
    }
}

void basefunc_std::removeAll(std::string &s, const char search) noexcept
{
    for (size_t pos = 0; ; pos += 1)
    {
        pos = s.find(search, pos);
        if (pos == std::string::npos) break;

        s.erase(pos, 1);
    }
}

std::string basefunc_std::read_file(const std::string& path)
{
    std::string ret;

    try
    {
        std::ifstream t(path);
        if (t)
        {
            std::string str_gz((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());
            ret = std::move(str_gz);
        }
    }
    catch(...) { }

    return ret;
}

bool basefunc_std::is_file(const std::string& path)
{
    struct stat path_stat;
    stat(path.c_str(), &path_stat);
    return S_ISREG(path_stat.st_mode);
}

bool basefunc_std::is_directory(const std::string& path)
{
    struct stat path_stat;
    stat(path.c_str(), &path_stat);
    return S_ISDIR(path_stat.st_mode);
}

/**
 *функция read_sec_parm() позволяет прочесть
 *заданный параметр секции
 *из заданного конфигурационного файла
 *
 *входные параметры:
 *	 filename - имя файла
 *	 sect_num - имя секции
 *	 parm_name - имя параметра
 *выходные:
 *	 save_buffer - буфер,куда следует сохранить параметр
 *возвращаемое значение - код ошибки
 */
int basefunc_std::read_settings(std::string fileName, std::string sect_name, std::string parm_name, std::string& save_buffer, bool exit_, bool cout_)
{
    FILE *cfg_file;	 /*указатель на конфигурационный файл*/
    char Order[100000];	 /*имя секции*/
    char buf[100000];	 /*временный буфер строки из файла*/
    int num_char=0;

    memset(Order,' ',sizeof(Order));

    save_buffer = "";

    /*открытие конфигурационного файла на чтение*/
    cfg_file=fopen(fileName.c_str(), "r");
    if(cfg_file==NULL)
    {
        basefunc_std::log("basefunc_std::read_settings cant open file " + fileName, "basefunc_std_read_settings");
        basefunc_std::cout("Cant open file " + fileName, "basefunc_std::read_settings", basefunc_std::COLOR::RED_COL);
        if (exit_) exit(0);
        return -1;
    }

    parm_name += "=";

    /*построчное сканирование файла*/
    while(fgets(buf,sizeof(buf),cfg_file)!=NULL)
    {
        int i=0;
        char ch=buf[0];/*временная переменная, в которую заносится символ*/

        /*
        *замеряется отступ от начала строки
        *(количество символов пробелов+табуляций)
        */
        while((ch==' ')||(ch=='\t')) ch=buf[++i];

        /*
        *если встретился символ коментариев или новая строка, то осуществляется
        *переход на следующую строку
        */
        if ((ch=='#')||(ch==';')||(ch=='\n')) continue;

        /*нахождение секции*/
        if(sscanf(buf+i,"[%s]",Order)!=0)	 continue;

        /*если секция не наша то дальше продолжается сканирование*/
        if (strncmp(sect_name.c_str(), Order, strlen(sect_name.c_str())) != 0)	continue;

        /*сравнение названий параметров*/
        if (strncmp(parm_name.c_str(), buf+i, strlen(parm_name.c_str()))!=0)	continue; // + sizeof('=')

        int j=i;/*номер последнего символа в строке*/
        ch=buf[j];

        /*символ ' ' является допустимым*/
        while((ch!='\t')&&(ch!=';')&&(ch!='#')&&(ch!='\n')&&(ch!='\r')) ch=buf[++j];
        num_char=j-(i+strlen(parm_name.c_str())); //+sizeof('=')

        char* buf_cpy = new char[num_char + 1];
        strncpy(buf_cpy, &buf[i + strlen(parm_name.c_str())], num_char); //+sizeof('=')
        buf_cpy[num_char] = 0;

        save_buffer = buf_cpy;

        delete [] buf_cpy;

        break;
    }
    /*закрытие файла*/
    fclose(cfg_file);

    if (num_char <= 0)
    {
        if (cout_)
        {
            basefunc_std::log("basefunc_std::read_settings cant read " + parm_name + " in section " + sect_name + " in file " + fileName, "basefunc_std_read_settings");
            basefunc_std::cout("Cant read " + parm_name + " in section " + sect_name + " in file " + fileName, "basefunc_std::read_settings", basefunc_std::COLOR::RED_COL);
        }

        if (exit_) exit(0);
    }

    return num_char;
}

void basefunc_std::set_timer_to_kill(std::int32_t sec_to_wait)
{
    std::string file_check { std::string(program_invocation_name) + "_autostop" };
    if (!basefunc_std::is_file(file_check))
    {
        std::string tm { std::to_string(time(0) + sec_to_wait) };
        basefunc_std::write_file_to_disk(file_check, tm);
    }
}

void basefunc_std::kill_check(int argc, char *argv[])
{
    if(argc >= 2) //Kill if command is stop
    {
        if (strcmp(argv[1], "stop") == 0) basefunc_std::killbrothers(1);
    }
    basefunc_std::killbrothers(0);
}

bool basefunc_std::killbrothers(const int flag) //Kill or find samename process
{
    DIR *dir;
    struct dirent *de;
    pid_t pid = 0;
    pid_t self = 0;
    char name[256], myname[256];
    if (!(dir = opendir ("/proc"))) // Open Dir /proc
    {
        std::cout << "Error to open base Dir process ..." << "/proc" << std::endl;
        return true;
    }

    self = getpid(); // Current Pid
    readname(myname, self); //Our name

    bool isExit = false;
    while ((de = readdir (dir)) != NULL) //Read all Dir
    {
        pid_t p { (pid_t) atoi (de->d_name) };

        if (!p || p == self) continue; //Continue if its not a process or its our PID

        readname(name, p); // считываем имя процесса

        if(strncmp(name, myname, 255) == 0) //If its the same our name
        {
            pid = p;

            if(flag == 1)
            {
                kill(pid, SIGTERM);
                //basefunc_std::execCMD("kill -9 " + std::to_string(pid));
            }
            isExit = true; //hujnja kakaja to
        }
    }

    closedir(dir);

    bool kill_9 { false };
    std::string file_check { std::string(program_invocation_name) + "_autostop" };

    if (flag != 1)
    {
        if (!isExit)
        {
            std::remove(file_check.c_str());
        }
    }

    if (basefunc_std::is_file(file_check))
    {
        std::int64_t tm { 0 };
        basefunc_std::stoi(basefunc_std::read_file(file_check), tm);
        if (tm > 0)
        {
            if (time(0) >= tm)
            {
                kill_9 = true;
                std::remove(file_check.c_str());
            }
        }
        else
        {
            std::remove(file_check.c_str());
        }
    }

    if(flag == 1)
    {
        exit(0); //if stop
    }
    else if (flag == 2)
    {
        if (kill_9)
        {
            kill(pid, SIGKILL);
            //basefunc_std::execCMD("kill -9 " + std::to_string(pid));
            //std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        return isExit;
    }
    else if(isExit) //if already running
    {
        if (kill_9)
        {
            kill(pid, SIGKILL);
            //basefunc_std::execCMD("kill -9 " + std::to_string(pid));
            //std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        std::cout << "programm is already running ..." << std::endl;
        exit(0); //hujnja kakaja to
    }

    return true;
}

void basefunc_std::readname(char *name, pid_t pid) //Read from  /proc/...
{
        char *pp = name, byte, path[256];
        int cntr = 0, file;
        sprintf (path, "/proc/%d/cmdline", pid);
        file = open(path, O_RDONLY);
        do{	// считываем имя без слешей
                read(file, &byte, 1);
                if (byte != '/') *pp++ = byte;
                else pp = name;
        }
        while(byte != EOF && byte != 0 && cntr++ < 255);
        close(file);
}

/**
 * РАСЖАТЬ КЛЮЧ
 */
std::string basefunc_std::uncompress_number(const std::string& k)
{
    std::string ret = "";

    int sz = k.size();
    for (int i = 0; i < sz; ++i)
    {
        char c = k.at(i);
        if ((c >= 65 && c <= 90) || (c >= 97 && c <= 99)) ret += std::to_string((int)c);
        else ret += k.at(i);
    }

    return ret;
}

/**
 * СЖАТЬ КЛЮЧ
 */
std::string basefunc_std::compress_number(const std::string& k)
{
    std::string ret = "";

    int sz = k.size();
    int i = 0;
    for (; i < sz - 1; i += 2)
    {
        int c = 0;
        if (basefunc_std::stoi(k.substr(i, 2), c))
        {
            if ((c >= 65 && c <= 90) || (c >= 97 && c <= 99)) ret += (char)c;
            else ret += k.substr(i, 2);
        }
        else ret += k.substr(i, 2);
    }

    if (i < sz) ret += k.substr(i);

    return ret;
}

/**
 * УРЛ ДЕКОДЕ
 */
std::string basefunc_std::url_decode(const std::string &SRC) noexcept
{
    std::string ret;
    char ch;
    unsigned int i, ii;
    for (i = 0; i < SRC.size(); ++i)
    {
        bool is_next_space { false };
        if (i + 1 < SRC.size())
        {
            if (SRC[i + 1] == ' ' || SRC[i + 1] == '%') is_next_space = true;
        }

        if (SRC[i] == '%' && !is_next_space)
        {
            sscanf(SRC.substr(i+1,2).c_str(), "%x", &ii);
            ch = static_cast<char>(ii);
            ret += ch;
            i = i + 2;
        }
        else
        {
            ret += SRC[i];
        }
    }
    return (ret);
}

std::string basefunc_std::url_encode(const std::string& value)
{
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (std::string::const_iterator i = value.begin(), n = value.end(); i != n; ++i) {
            std::string::value_type c = (*i);

            // Keep alphanumeric and other accepted characters intact
            if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
                escaped << c;
                continue;
            }

            // Any other characters are percent-encoded
            escaped << std::uppercase;
            escaped << '%' << std::setw(2) << int((unsigned char) c);
            escaped << std::nouppercase;
    }

    return escaped.str();
}

/**
 * РАССТОЯНИЕ МЕЖДУ ТОЧКАМИ В МЕТРАХ
 */
int basefunc_std::distance_map(double a1, double b1, double a2, double b2)
{
    //to radians
    long double lat1 = a1 * M_PI / 180;
    long double lat2 = a2 * M_PI / 180;
    long double long1 = b1 * M_PI / 180;
    long double long2 = b2 * M_PI / 180;

    long double cl1 = cos(lat1);
    long double cl2 = cos(lat2);
    long double sl1 = sin(lat1);
    long double sl2 = sin(lat2);

    long double delta = long2 - long1;
    long double cdelta = cos(delta);
    long double sdelta = sin(delta);

    long double y = sqrt(pow(cl2 * sdelta, 2) + pow(cl1 * sl2 - sl1 * cl2 * cdelta, 2));
    long double x = sl1 * sl2 + cl1 * cl2 * cdelta;

    long double ad = atan2(y, x);
    long double dist = ad * 6372795;

    return (int) round(dist);
}

std::wstring basefunc_std::utf8_to_utf16(const std::string& utf8)
{
    std::vector<unsigned long> unicode;
    size_t i = 0;
    while (i < utf8.size())
    {
        unsigned long uni;
        size_t todo;
        unsigned char ch = utf8[i++];
        if (ch <= 0x7F)
        {
            uni = ch;
            todo = 0;
        }
        else if (ch <= 0xBF)
        {
            throw std::logic_error("not a UTF-8 string");
        }
        else if (ch <= 0xDF)
        {
            uni = ch&0x1F;
            todo = 1;
        }
        else if (ch <= 0xEF)
        {
            uni = ch&0x0F;
            todo = 2;
        }
        else if (ch <= 0xF7)
        {
            uni = ch&0x07;
            todo = 3;
        }
        else
        {
            throw std::logic_error("not a UTF-8 string");
        }
        for (size_t j = 0; j < todo; ++j)
        {
            if (i == utf8.size())
                throw std::logic_error("not a UTF-8 string");
            unsigned char ch = utf8[i++];
            if (ch < 0x80 || ch > 0xBF)
                throw std::logic_error("not a UTF-8 string");
            uni <<= 6;
            uni += ch & 0x3F;
        }
        if (uni >= 0xD800 && uni <= 0xDFFF)
            throw std::logic_error("not a UTF-8 string");
        if (uni > 0x10FFFF)
            throw std::logic_error("not a UTF-8 string");
        unicode.push_back(uni);
    }
    std::wstring utf16;
    for (size_t i = 0; i < unicode.size(); ++i)
    {
        unsigned long uni = unicode[i];
        if (uni <= 0xFFFF)
        {
            utf16 += (wchar_t)uni;
        }
        else
        {
            uni -= 0x10000;
            utf16 += (wchar_t)((uni >> 10) + 0xD800);
            utf16 += (wchar_t)((uni & 0x3FF) + 0xDC00);
        }
    }
    return utf16;
}

std::string basefunc_std::format(const std::string fmt_str, ...)
{
    //Reserve two times as much as the length of the fmt_str
    int final_n, n = ((int)fmt_str.size()) * 2;
    std::unique_ptr<char[]> formatted;
    va_list ap;
    while(1)
    {
        //Wrap the plain char array into the unique_ptr
        formatted.reset(new char[n]);
        strcpy(&formatted[0], fmt_str.c_str());
        va_start(ap, fmt_str);
        final_n = vsnprintf(&formatted[0], n, fmt_str.c_str(), ap);
        va_end(ap);
        if (final_n < 0 || final_n >= n)
            n += abs(final_n - n + 1);
        else
            break;
    }
    return std::string(formatted.get());
}

void basefunc_std::cout_mem_usage()
{
    long rss, vm;
    basefunc_std::process_mem_usage(vm, rss);
    basefunc_std::cout(std::to_string(rss) + "Mb", "rss");
}

void basefunc_std::process_mem_usage(long& vm_usage, long& resident_set)
{
   using std::ios_base;
   using std::ifstream;
   using std::string;

   vm_usage     = 0;
   resident_set = 0;

   // 'file' stat seems to give the most reliable results
   //
   ifstream stat_stream("/proc/self/stat",ios_base::in);

   // dummy vars for leading entries in stat that we don't care about
   //
   string pid, comm, state, ppid, pgrp, session, tty_nr;
   string tpgid, flags, minflt, cminflt, majflt, cmajflt;
   string utime, stime, cutime, cstime, priority, nice;
   string O, itrealvalue, starttime;

   // the two fields we want
   //
   unsigned long vsize;
   long rss;

   stat_stream >> pid >> comm >> state >> ppid >> pgrp >> session >> tty_nr
               >> tpgid >> flags >> minflt >> cminflt >> majflt >> cmajflt
               >> utime >> stime >> cutime >> cstime >> priority >> nice
               >> O >> itrealvalue >> starttime >> vsize >> rss; // don't care about the rest

   stat_stream.close();

   long page_size_kb = sysconf(_SC_PAGE_SIZE) / 1024; // in case x86-64 is configured to use 2MB pages
   vm_usage     = vsize / (1024 * 1024);
   resident_set = (rss * page_size_kb) / 1024;
}
