#pragma once

#include <string>
#include <vector>

class ModuleHandler {
protected:
    std::string module_name;

    // Internal helper methods
    enum class SignalType { Term, Kill };
    std::vector<int> find_module_pids() const;
    bool send_signal(int pid, SignalType signal) const;
    bool is_process_alive(int pid) const;
    virtual std::vector<std::string> get_backup_files() const;
    virtual bool backup_files() const;
    virtual bool restore_files() const;
    virtual bool recover_hardware() const;

public:
    explicit ModuleHandler(const std::string& name) : module_name(name) {}
    virtual ~ModuleHandler() = default;

    virtual bool start(const std::vector<std::string>& extra_args);
    virtual bool stop();
    virtual bool preinst();
    virtual bool postinst();
    virtual bool preun();
    virtual bool postun();
    bool update_sign();
};
