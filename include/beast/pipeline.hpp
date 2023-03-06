#ifndef BEAST_PIPELINE_HPP_
#define BEAST_PIPELINE_HPP_

// Standard
#include <list>
#include <memory>

// BEAST
#include <beast/pipe.hpp>

namespace beast {

/**
 * @class Pipeline
 * @brief Work management class orchestrating `Pipe` class instances
 *
 * TODO(fairlight1337): Document this class.
 *
 * @author Jan Winkler
 * @date 2023-03-06
 */
class Pipeline {
 public:
  struct Connection {
    std::shared_ptr<Pipe> source_pipe_;
    uint32_t source_slot_index;

    std::shared_ptr<Pipe> destination_pipe_;
    uint32_t destination_slot_index;
  };

  void addPipe(const std::shared_ptr<Pipe>& pipe);

  void connectPipes(
      const std::shared_ptr<Pipe>& source_pipe, uint32_t source_slot_index,
      const std::shared_ptr<Pipe>& destination_pipe, uint32_t destination_slot_index);

  const std::list<std::shared_ptr<Pipe>>& getPipes() const;

  const std::list<Connection>& getConnections() const;

 private:
  std::list<std::shared_ptr<Pipe>> pipes_;

  std::list<Connection> connections_;
};

}  // namespace beast

#endif  // BEAST_PIPELINE_HPP_
