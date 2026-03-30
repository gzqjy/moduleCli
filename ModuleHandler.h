#pragma once

#include <string>
#include <vector>

class ModuleHandler {
protected:
    std::string module_name;

    // Internal helper methods
    std::vector<int> find_module_pids() const;
    bool send_signal(int pid, const std::string& signal) const;
    bool is_process_alive(int pid) const;
    bool backup_files() const;
    bool restore_files() const;
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
};
