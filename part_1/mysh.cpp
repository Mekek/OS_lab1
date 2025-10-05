#include <bits/stdc++.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <signal.h>
#include <time.h>

using namespace std;

// чистим зомби-процессы
static void sigchld_handler(int) {
    while (waitpid(-1, nullptr, WNOHANG) > 0) {}
}

vector<string> split_tokens(const string &line) {
    vector<string> toks;
    istringstream iss(line);
    string t;
    while (iss >> t) toks.push_back(t);
    return toks;
}

int main() {
    // ставим обработчик SIGCHLD, чтобы не плодить зомби
    struct sigaction sa{};
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, nullptr);

    string line;
    while (true) {
        cout << "mysh$ " << flush;
        if (!getline(cin, line)) break;
        if (line.empty()) continue;

        auto toks = split_tokens(line);
        if (toks.empty()) continue;

        // встроенные команды 
        if (toks[0] == "exit") {
            cout << "Bye!\n";
            break;
        }

        if (toks[0] == "pwd") {
            char cwd[1024];
            if (getcwd(cwd, sizeof(cwd)) != nullptr)
                cout << cwd << "\n";
            else
                perror("pwd");
            continue;
        }

        if (toks[0] == "cd") {
            const char* dir = toks.size() > 1 ? toks[1].c_str() : getenv("HOME");
            if (chdir(dir) != 0) perror("cd");
            continue;
        }

        if (toks[0] == "echo") {
            for (size_t i = 1; i < toks.size(); ++i)
                cout << toks[i] << (i + 1 < toks.size() ? " " : "");
            cout << "\n";
            continue;
        }

        bool background = false;
        if (!toks.empty() && toks.back() == "&") {
            background = true;
            toks.pop_back();
            if (toks.empty()) continue;
        }

        vector<char*> argv;
        for (auto &s : toks) argv.push_back(const_cast<char*>(s.c_str()));
        argv.push_back(nullptr);

        struct timespec tstart{}, tend{};
        clock_gettime(CLOCK_MONOTONIC, &tstart);

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            continue;
        }

        if (pid == 0) {
            execvp(argv[0], argv.data());
            perror("execvp");
            _exit(127);
        } else {
            if (background) {
                cout << "[bg] PID " << pid << "\n";
                continue;
            }

            int status = 0;
            struct rusage usage{};
            wait4(pid, &status, 0, &usage);
            clock_gettime(CLOCK_MONOTONIC, &tend);

            double real = (tend.tv_sec - tstart.tv_sec)
                        + (tend.tv_nsec - tstart.tv_nsec)/1e9;
            double user = usage.ru_utime.tv_sec + usage.ru_utime.tv_usec/1e6;
            double sys  = usage.ru_stime.tv_sec + usage.ru_stime.tv_usec/1e6;

            cout << "PID " << pid << " done. real=" << real
                 << "s, user=" << user << "s, sys=" << sys << "s\n";
            cout << "ctx switches: voluntary=" << usage.ru_nvcsw
                 << ", involuntary=" << usage.ru_nivcsw << "\n";

            if (WIFEXITED(status))
                cout << "exit code: " << WEXITSTATUS(status) << "\n";
            else if (WIFSIGNALED(status))
                cout << "killed by signal: " << WTERMSIG(status) << "\n";
        }
    }

    return 0;
}
