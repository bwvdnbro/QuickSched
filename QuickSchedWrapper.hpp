/**
 * @file QuickSchedWrapper.hpp
 *
 * @brief Class wrappers around quicksched functionality.
 *
 * @author Bert Vandenbroucke (bert.vandenbroucke@ugent.be)
 */
#ifndef QUICKSCHEDWRAPPER_HPP
#define QUICKSCHEDWRAPPER_HPP

#include "CPUCycle.hpp"
#include "quicksched/quicksched.h"

#include <cinttypes>
#include <map>
#include <ostream>
#include <typeinfo>

/**
 * @brief Task interface.
 */
class Task {
private:
  /*! @brief Corresponding QuickSched task. */
  qsched_task_t _task_id;

public:
  /**
   * @brief Empty constructor.
   */
  inline Task() : _task_id(0) {}

  /**
   * @brief Execute the task.
   */
  virtual void execute() = 0;

  /**
   * @brief Set the QuickSched task ID of the task.
   *
   * @param task_id QuickSched task ID.
   */
  inline void set_id(const qsched_task_t task_id) { _task_id = task_id; }

  /**
   * @brief Get the QuickSched task ID of the task.
   *
   * @return QuickSched task ID.
   */
  inline qsched_task_t get_id() const { return _task_id; }

  /**
   * @brief Print out information about the task execution.
   *
   * @param scheduler Pointer to the QuickSched scheduler that contains all the
   * relevant information.
   * @param stream std::ostream to write to.
   */
  inline void print(const struct qsched &scheduler,
                    std::ostream &stream) const {
    const struct task &task_data = scheduler.tasks[_task_id];
    stream << task_data.qid << "\t" << task_data.tic << "\t" << task_data.toc
           << "\t" << task_data.type << "\n";
  }
};

/**
 * @brief Wrapped task.
 */
class WrappedTask {
private:
  /*! @brief Pointer to the actual task. */
  Task *_task;

public:
  /**
   * @brief Constructor.
   *
   * @param task Pointer to the task that is wrapped.
   */
  inline WrappedTask(Task *task) : _task(task) {}

  /**
   * @brief Execute the wrapped task.
   */
  inline void execute() { _task->execute(); }
};

/**
 * @brief Resource interface.
 */
class Resource {
private:
  /*! @brief Corresponding QuickSched resource. */
  qsched_res_t _resource_id;

public:
  /**
   * @brief Set the QuickSched resource ID of the resource.
   *
   * @param resource_id QuickSched resource ID.
   */
  inline void set_id(const qsched_res_t resource_id) {
    _resource_id = resource_id;
  }

  /**
   * @brief Get the QuickSched resource ID of the resource.
   *
   * @return QuickSched resource ID.
   */
  inline qsched_res_t get_id() const { return _resource_id; }
};

/**
 * @brief Class wrapper around the QuickSched library.
 */
class QuickSched {
private:
  /*! @brief Number of threads to use during parallel execution.*/
  const int_fast32_t _number_of_threads;

  /*! @brief QuickSched scheduler. */
  struct qsched _s;

  /*! @brief Task type counter. */
  int_fast32_t _task_type;

  /*! @brief Task type dictionary. */
  std::map<std::string, int_fast32_t> _type_dict;

public:
  /**
   * @brief Runner function passed on to the QuickSched library.
   *
   * @param task_type Type of task being executed.
   * @param wrapped_task Void pointer to the wrapped task.
   */
  inline static void execute_task(int task_type, void *wrapped_task) {
    WrappedTask *task = static_cast<WrappedTask *>(wrapped_task);
    task->execute();
  }

public:
  /**
   * @brief Constructor.
   *
   * @param number_of_threads Number of threads to use during parallel
   * execution.
   */
  inline QuickSched(const int_fast32_t number_of_threads)
      : _number_of_threads(number_of_threads), _task_type(0) {
    bzero(&_s, sizeof(struct qsched));
    qsched_init(&_s, number_of_threads, qsched_flag_none);
  }

  /**
   * @brief Destructor.
   *
   * Free memory used by library.
   */
  inline ~QuickSched() { qsched_free(&_s); }

  /**
   * @brief Execute all tasks that were added previously.
   *
   * @param number_of_threads Number of threads to use.
   */
  inline void execute_tasks(int_fast32_t number_of_threads = -1) {
    if (number_of_threads <= 0) {
      number_of_threads = _number_of_threads;
    }
    qsched_run(&_s, number_of_threads, execute_task);
  }

  /**
   * @brief Register the given resource with the QuickSched library.
   *
   * @param resource Resource to register.
   */
  inline void register_resource(Resource &resource) {
    const qsched_res_t rid =
        qsched_addres(&_s, qsched_owner_none, qsched_res_none);
    resource.set_id(rid);
  }

  /**
   * @brief Register the given task with the QuickSched library.
   *
   * @param task Task to register.
   */
  inline void register_task(Task &task) {

    WrappedTask wrapper(&task);
    int_fast32_t task_type;
    const std::string task_type_name = typeid(task).name();
    auto it = _type_dict.find(task_type_name);
    if (it != _type_dict.end()) {
      task_type = it->second;
    } else {
      task_type = _task_type;
      _type_dict[task_type_name] = _task_type;
      ++_task_type;
    }
    const qsched_task_t tid = qsched_addtask(&_s, task_type, task_flag_none,
                                             &wrapper, sizeof(WrappedTask), 1);
    task.set_id(tid);
  }

  /**
   * @brief Make a link between the given task and the given resource.
   *
   * @param task Task that accesses the resource.
   * @param resource Resource that is accessed.
   * @param write_access Whether or not the task needs write access to the
   * resource.
   */
  inline void link_task_and_resource(const Task &task, const Resource &resource,
                                     const bool write_access) {
    if (write_access) {
      qsched_addlock(&_s, task.get_id(), resource.get_id());
    } else {
      qsched_adduse(&_s, task.get_id(), resource.get_id());
    }
  }

  /**
   * @brief Create a link between the first task and the second task, so that
   * the second task can only be executed once the first finished.
   *
   * @param first_task First task: needs to run first.
   * @param second_task Second task: can only run after the first.
   */
  inline void link_tasks(const Task &first_task, const Task &second_task) {
    qsched_addunlock(&_s, first_task.get_id(), second_task.get_id());
  }

  /**
   * @brief Print the task info for the given task to the given stream.
   *
   * @param task Task to print.
   * @param stream std::ostream to write to.
   */
  inline void print_task(const Task &task, std::ostream &stream) {
    task.print(_s, stream);
  }

  /**
   * @brief Print the task type dictionary to the given stream.
   *
   * @param stream std::ostream to write to.
   */
  inline void print_type_dict(std::ostream &stream) {
    for (auto it = _type_dict.begin(); it != _type_dict.end(); ++it) {
      stream << it->second << "\t" << it->first << "\n";
    }
  }
};

#endif // QUICKSCHEDWRAPPER_HPP
