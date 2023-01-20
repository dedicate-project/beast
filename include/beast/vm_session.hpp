#ifndef BEAST_VM_SESSION_HPP_
#define BEAST_VM_SESSION_HPP_

// Standard
#include <cstdint>
#include <map>

// Internal
#include <beast/program.hpp>

namespace beast {

/**
 * @class VmSession
 * @brief Holds the current instruction pointer, variable memory, and string table for programs
 *
 * This class holds a program definition and defines the size and content of an associated variable
 * memory and string table. In addition to that, the current instruction pointer location for
 * executing the associated program is held in this class. VirtualMachine class instances
 * (specifically CpuVirtualMachine) use VmSession instances to access the next portion of an
 * executed program, and to steer instruction pointer movement. Also, the VirtualMachine classes
 * access the VmSession's variable memory and string table contents when executing a Program
 * instance's code.
 */
class VmSession {
 public:
  /**
   * @brief Describes the intended I/O behavior of a variable
   *
   * A variable can either be used only for in-memory storage, or for input/output operations. In
   * each case, a variable needs to be declared by a program. A program itself cannot change a
   * variable's behavior, but can query which behavior is intended and whether the variable was
   * changed since the last interaction. The behavior needs to be set from outside the program. An
   * input variable that is set from outside of the program allows a program to perform actions
   * based on whether new input is available. An output variable can be set from inside the program,
   * and outside of the program an executing entity can check whether new output is available.
   *
   * Per default, all variables are defined as store only. All variable behaviors allow storing data
   * independent from their intended I/O behavior.
   */
  enum class VariableIoBehavior {
    Store = 0,  ///< Variable is used for in-memory storage only, no I/O behavior is expected
    Input = 1,  ///< Variable is expected to receive input from outside
    Output = 2  ///< Variable is expected to be read from outside
  };

  /**
   * @brief Contains metadata about a declared variable
   *
   * For each declared variable, data about its type, intended behavior, and current behavioral
   * state is stored. This struct describes these metadata elements.
   */
  struct VariableDescriptor {
    Program::VariableType type;           ///< The declared type of the variable
    VariableIoBehavior behavior;          ///< The intended behavior of the variable
    bool changed_since_last_interaction;  ///< Whether the value was changed since it was last
                                          ///  written/read, based on the intended behavior
  };

  /**
   * @fn VmSession::VmSession
   * @brief Standard constructor
   *
   * Requires a Program instance, and the definition of variable memory and string table size. The
   * `variable_count` parameter represents the number of allowed variables (indexed starting at 0,
   * consecutively). The `string_table_count` and `max_string_size` parameters represent the number
   * of string table items possible, and the maximum length per string table item, respectively.
   *
   * @param program The Program instance to associate to this VmSession instance
   * @param variable_count The maximum number of variables
   * @param string_table_count The maximum number of string table items
   * @param max_string_size The maximum length per string table item
   */
  VmSession(
      Program program, size_t variable_count, size_t string_table_count,
      size_t max_string_size);

  /**
   * @fn VmSession::setVariableBehavior
   * @brief Sets the I/O behavior for a variable
   *
   * A variable can have three I/O behaviors: Store only, store and input, or store and output (see
   * VariableIoBehavior for more details). For store only variables, no specific behavior is
   * expected. Input variables can be checked by a program via the checkIfInputWasSet operator. If
   * from outside of the program an input variable was set via setVariableValue, this operator will
   * yield a positive response (independent from the value that was set). For output variables, it
   * works the other way around: When programs set such a variable, functions outside of the program
   * can retrieve this status by using the hasOutputDataAvailable call (and retrieve the data via
   * getVariableValue). All variables declared through setVariableBehavior can be used like any
   * other variable by all operators.
   *
   * When calling this method on a variable, this `variable_index` is registered in the VmSession
   * instance's variable memory. This means that it can't (and doesn't have to) be declared again by
   * a Program instance and can be used as if it was declared already. It can be undeclared like any
   * other previously declared variable.
   *
   * @param variable_index The variable to set the behavior for
   * @param behavior The I/O behavior to set for the variable
   * @sa getVariableBehavior()
   */
  void setVariableBehavior(int32_t variable_index, VariableIoBehavior behavior);

  /**
   * @fn VmSession::getVariableBehavior
   * @brief Returns the I/O behavior currently set for a variable
   *
   * Returns which variable behavior is currently defined for the variable at index `variable_index`
   * (potentially resolving its links of `follow_links` is set). For details on the exact I/O
   * behaviors, see setVariableBehavior.
   *
   * @param variable_index The variable to return the I/O behavior for
   * @param follow_links Whether to resolve the variable's links
   * @sa setVariableBehavior()
   */
  VariableIoBehavior getVariableBehavior(int32_t variable_index, bool follow_links);

  /**
   * @fn VmSession::hasOutputDataAvailable
   * @brief Checks if an output variable has new data available
   *
   * If, since the last read attempt, data was written to the passed in variable index, `true` is
   * returned, and `false` otherwise.
   *
   * @param variable_index The variable index to check for new output
   * @param follow_links Whether to resolve the variable's links
   * @return Returns `true` if data was written to the variable, `false` otherwise
   */
  bool hasOutputDataAvailable(int32_t variable_index, bool follow_links);

  /**
   * @fn VmSession::setMaximumPrintBufferLength
   * @brief Sets the maximum length in characters that the print buffer can hold
   *
   * If the print buffer reaches a length of more than this value, an exception is thrown. This can
   * be prevented by regularly calling clearPrintBuffer.
   */
  void setMaximumPrintBufferLength(size_t maximum_print_buffer_length);

  /**
   * @fn VmSession::getData4
   * @brief Return the next 4 bytes of program byte code
   *
   * @return The next 4 bytes of program byte code
   */
  int32_t getData4();

  /**
   * @fn VmSession::getData2
   * @brief Return the next 2 bytes of program byte code
   *
   * @return The next 2 bytes of program byte code
   */
  int16_t getData2();

  /**
   * @fn VmSession::getData1
   * @brief Return the next 1 byte of program byte code
   *
   * @return The next 1 byte of program byte code
   */
  int8_t getData1();

  /**
   * @fn VmSession::getVariableValue
   * @brief Returns the stored value of a variable
   *
   * The value returned is read from the variable at `variable_index`, resolving its links if
   * `follow_links` is set.
   *
   * @param variable_index The variable to return the stored value for
   * @param follow_links Whether to resolve the variable's links
   * @sa setVariableValue()
   */
  int32_t getVariableValue(int32_t variable_index, bool follow_links);

  /**
   * @fn VmSession::setVariableValue
   * @brief Sets the value stored in a variable
   *
   * The new value `value` will be stored in the variable at the index `variable_index`, resolving
   * its links if `follow_links` is set.
   *
   * @param variable_index The variable to store value in
   * @param follow_links Whether to resolve the variable's links
   * @param value The value to store in the variable
   */
  void setVariableValue(int32_t variable_index, bool follow_links, int32_t value);

  /**
   * @fn VmSession::isAtEnd
   * @brief Checks whether the associated program is at the end of its executable code
   *
   * Effectively, this function returns whether the instruction pointer points beyond the end of the
   * code.
   *
   * @return Boolean flag denoting whether the program execution is at its end
   */
  bool isAtEnd() const;

  /**
   * @fn VmSession::registerVariable
   * @brief Registers a variable at index and type in the variable memory
   *
   * See Program::declareVariable for the intended operator use.
   *
   * @param variable_index The index to declare the variable at.
   * @param variable_type The designated type of the variable.
   * @sa unregisterVariable()
   */
  void registerVariable(int32_t variable_index, Program::VariableType variable_type);

  /**
   * @fn VmSession::getRealVariableIndex
   * @brief Resolves a variable index based on linkage
   *
   * The first variable index that is not a link is returned. This method detects circular linkage,
   * and throws if either this is detected or if any variable index points outside of the valid
   * variable memory.
   *
   * @param variable_index The index of the start variable.
   * @param follow_links Whether to resolve variable links.
   */
  int32_t getRealVariableIndex(int32_t variable_index, bool follow_links);

  /**
   * @fn VmSession::setVariable
   * @brief Sets the value of a variable
   *
   * Sets the value of a variable at the given index to the value `content`. The variable index
   * needs to be declared prior to this.
   *
   * See Program::setVariable for the intended operator use.
   *
   * @param variable_index The index of the variable.
   * @param value The value to assign to the variable.
   * @param follow_links Whether to resolve variable links.
   */
  void setVariable(int32_t variable_index, int32_t value, bool follow_links);

  /**
   * @fn VmSession::unregisterVariable
   * @brief Removes a registered variable from the internal variable memory
   *
   * See Program::undeclareVariable for the intended operator use.
   *
   * @param variable_index The index of the variable to undeclare.
   * @sa registerVariable()
   */
  void unregisterVariable(int32_t variable_index);

  /**
   * @fn VmSession::setStringTableEntry
   * @brief Sets the content of a string table entry
   *
   * See Program::setStringTableEntry for the intended operator use.
   *
   * @param string_table_index The string table index to set.
   * @param string The content to store in the string table.
   */
  void setStringTableEntry(int32_t string_table_index, std::string_view string_content);

  /**
   * @fn VmSession::getStringTableEntry
   * @brief Returns the string value stored at the given index in the string table
   *
   * @param string_table_index The string table index to read the string from
   * @return The string read from the string table at the specified index
   */
  const std::string& getStringTableEntry(int32_t string_table_index) const;

  /**
   * @fn VmSession::appendToPrintBuffer
   * @brief Appends a string to the current print buffer
   *
   * The print buffer is a store for all characters that should be printed to screen. This method
   * appends a given string to the already existing content.
   *
   * @param string The string to append to the print buffer.
   * @sa appendVariableToPrintBuffer(), getPrintBuffer(), clearPrintBuffer()
   */
  void appendToPrintBuffer(std::string_view string);

  /**
   * @fn VmSession::appendVariableToPrintBuffer
   * @brief Appends the value of a variable to the print buffer
   *
   * The value can be added to the print buffer either via its numeric value or as the character
   * value of the least significant byte of the variable's value.
   *
   * @param variable_index The variable to read the value from
   * @param follow_links Whether to resolve variable links
   * @param as_char Whether to append the LSB char value to the buffer (`true`) or the numeric value
   *        of the entire `int32_t` (`false`)
   * @sa appendToPrintBuffer(), getPrintBuffer(), clearPrintBuffer()
   */
  void appendVariableToPrintBuffer(int32_t variable_index, bool follow_links, bool as_char);

  /**
   * @fn VmSession::getPrintBuffer
   * @brief Returns a reference to the print buffer
   *
   * @sa appendToPrintBuffer(), appendVariableToPrintBuffer(), clearPrintBuffer()
   */
  const std::string& getPrintBuffer() const;

  /**
   * @fn VmSession::clearPrintBuffer
   * @brief Cleas the print buffer
   *
   * @sa appendToPrintBuffer(), appendVariableToPrintBuffer(), getPrintBuffer()
   */
  void clearPrintBuffer();

  /**
   * @fn VmSession::terminate
   * @brief Marks the program as terminates and sets its return code
   *
   * To prevent further execution, this call marks the program as terminated internally. The passed
   * in return code is stored.
   *
   * @param return_code The return code to store for the program
   * @sa getReturnCode(), terminateWithVariableReturnCode()
   */
  void terminate(int8_t return_code);

  /**
   * @fn VmSession::getReturnCode
   * @brief Returns the return code for a program
   *
   * This function returns the return code set by any terminate call. If this function is called
   * before the program ends, its returned value will be `0x0`.
   */
  int8_t getReturnCode() const;

  /**
   * @fn VmSession::addConstantToVariable
   * @brief Adds a constant to a variable's value
   *
   * See Program::addConstantToVariable for the intended operator use.
   *
   * @param variable_index The variable to add the constant to
   * @param constant The constant to add to the variable
   * @param follow_links Wehether to resolve the variable's links
   */
  void addConstantToVariable(int32_t variable_index, int32_t constant, bool follow_links);

  /**
   * @fn VmSession::addVariableToVariable
   * @brief Add the value of one variable to another variable
   *
   * See Program::addVariableToVariable for the intended operator use.
   *
   * @param source_variable The variable to add to the other variable
   * @param destination_variable The variable to add the value to
   * @param follow_source_links Whether to resolve the source variable's links
   * @param follow_destination_links Whether to resolve the destination variable's links
   */
  void addVariableToVariable(
      int32_t source_variable, int32_t destination_variable,
      bool follow_source_links, bool follow_destination_links);

  /**
   * @fn VmSession::subtractConstantFromVariable
   * @brief Subtract a constant from a variable
   *
   * See Program::subtractConstantFromVariable for the intended operator use.
   *
   * @param variable_index The variable to subtract the constant from
   * @param constant The constant to subtract from the variable
   * @param follow_links Wehether to resolve the variable's links
   */
  void subtractConstantFromVariable(int32_t variable_index, int32_t constant, bool follow_links);

  /**
   * @fn VmSession::subtractVariableFromVariable
   * @brief Subtract a variable from a variable
   *
   * See Program::subtractVariableFromVariable for the intended operator use.
   *
   * @param source_variable The variable to subtract from the other variable
   * @param destination_variable The variable to subtract the value from
   * @param follow_source_links Whether to resolve the source variable's links
   * @param follow_destination_links Whether to resolve the destination variable's links
   */
  void subtractVariableFromVariable(
      int32_t source_variable, int32_t destination_variable,
      bool follow_source_links, bool follow_destination_links);

  /**
   * @fn VmSession::relativeJumpToVariableAddressIfVariableGt0
   * @brief NEEDS DOCUMENTATION
   *
   * See Program::relativeJumpToVariableAddressIfVariableGt0 for the intended operator use.
   *
   * Perform a relative jump to a variable address if a condition variable is > 0.
   *
   * @param condition_variable The variable holding the value to compare
   * @param follow_condition_links Whether to resolve the comparison variable's links
   * @param addr_variable The variable holding the target address
   * @param follow_addr_links Whether to resolve the target address variable's links
   */
  void relativeJumpToVariableAddressIfVariableGt0(
      int32_t condition_variable, bool follow_condition_links,
      int32_t addr_variable, bool follow_addr_links);

  /**
   * @fn VmSession::relativeJumpToVariableAddressIfVariableLt0
   * @brief NEEDS DOCUMENTATION
   *
   * See Program::relativeJumpToVariableAddressIfVariableLt0 for the intended operator use.
   *
   * Perform a relative jump to a variable address if a condition variable is < 0.
   *
   * @param condition_variable The variable holding the value to compare
   * @param follow_condition_links Whether to resolve the comparison variable's links
   * @param addr_variable The variable holding the target address
   * @param follow_addr_links Whether to resolve the target address variable's links
   */
  void relativeJumpToVariableAddressIfVariableLt0(
      int32_t condition_variable, bool follow_condition_links,
      int32_t addr_variable, bool follow_addr_links);

  /**
   * @fn VmSession::relativeJumpToVariableAddressIfVariableEq0
   * @brief NEEDS DOCUMENTATION
   *
   * See Program::relativeJumpToVariableAddressIfVariableEq0 for the intended operator use.
   *
   * Perform a relative jump to a variable address if a condition variable is = 0.
   *
   * @param condition_variable The variable holding the value to compare
   * @param follow_condition_links Whether to resolve the comparison variable's links
   * @param addr_variable The variable holding the target address
   * @param follow_addr_links Whether to resolve the target address variable's links
   */
  void relativeJumpToVariableAddressIfVariableEq0(
      int32_t condition_variable, bool follow_condition_links,
      int32_t addr_variable, bool follow_addr_links);

  /**
   * @fn VmSession::absoluteJumpToVariableAddressIfVariableGt0
   * @brief NEEDS DOCUMENTATION
   *
   * See Program::absoluteJumpToVariableAddressIfVariableGt0 for the intended operator use.
   *
   * Perform an absolute jump to a variable address if a condition variable is > 0.
   *
   * @param condition_variable The variable holding the value to compare
   * @param follow_condition_links Whether to resolve the comparison variable's links
   * @param addr_variable The variable holding the target address
   * @param follow_addr_links Whether to resolve the target address variable's links
   */
  void absoluteJumpToVariableAddressIfVariableGt0(
      int32_t condition_variable, bool follow_condition_links,
      int32_t addr_variable, bool follow_addr_links);

  /**
   * @fn VmSession::absoluteJumpToVariableAddressIfVariableLt0
   * @brief NEEDS DOCUMENTATION
   *
   * See Program::absoluteJumpToVariableAddressIfVariableLt0 for the intended operator use.
   *
   * Perform an absolute jump to a variable address if a condition variable is < 0.
   *
   * @param condition_variable The variable holding the value to compare
   * @param follow_condition_links Whether to resolve the comparison variable's links
   * @param addr_variable The variable holding the target address
   * @param follow_addr_links Whether to resolve the target address variable's links
   */
  void absoluteJumpToVariableAddressIfVariableLt0(
      int32_t condition_variable, bool follow_condition_links,
      int32_t addr_variable, bool follow_addr_links);

  /**
   * @fn VmSession::absoluteJumpToVariableAddressIfVariableEq0
   * @brief NEEDS DOCUMENTATION
   *
   * See Program::absoluteJumpToVariableAddressIfVariableEq0 for the intended operator use.
   *
   * Perform an absolute jump to a variable address if a condition variable is = 0.
   *
   * @param condition_variable The variable holding the value to compare
   * @param follow_condition_links Whether to resolve the comparison variable's links
   * @param addr_variable The variable holding the target address
   * @param follow_addr_links Whether to resolve the target address variable's links
   */
  void absoluteJumpToVariableAddressIfVariableEq0(
      int32_t condition_variable, bool follow_condition_links,
      int32_t addr_variable, bool follow_addr_links);

  /**
   * @fn VmSession::relativeJumpToAddressIfVariableGt0
   * @brief NEEDS DOCUMENTATION
   *
   * See Program::relativeJumpToAddressIfVariableGt0 for the intended operator use.
   *
   * Perform a relative jump to a fixed address if a condition variable is > 0.
   *
   * @param condition_variable The variable holding the value to compare
   * @param follow_condition_links Whether to resolve the comparison variable's links
   * @param addr The target address
   */
  void relativeJumpToAddressIfVariableGt0(
      int32_t condition_variable, bool follow_condition_links, int32_t addr);

  /**
   * @fn VmSession::relativeJumpToAddressIfVariableLt0
   * @brief NEEDS DOCUMENTATION
   *
   * See Program::relativeJumpToAddressIfVariableLt0 for the intended operator use.
   *
   * Perform a relative jump to a fixed address if a condition variable is < 0.
   *
   * @param condition_variable The variable holding the value to compare
   * @param follow_condition_links Whether to resolve the comparison variable's links
   * @param addr The target address
   */
  void relativeJumpToAddressIfVariableLt0(
      int32_t condition_variable, bool follow_condition_links, int32_t addr);

  /**
   * @fn VmSession::relativeJumpToAddressIfVariableEq0
   * @brief NEEDS DOCUMENTATION
   *
   * See Program::relativeJumpToAddressIfVariableEq0 for the intended operator use.
   *
   * Perform a relative jump to a fixed address if a condition variable is = 0.
   *
   * @param condition_variable The variable holding the value to compare
   * @param follow_condition_links Whether to resolve the comparison variable's links
   * @param addr The target address
   */
  void relativeJumpToAddressIfVariableEq0(
      int32_t condition_variable, bool follow_condition_links, int32_t addr);

  /**
   * @fn VmSession::absoluteJumpToAddressIfVariableGt0
   * @brief NEEDS DOCUMENTATION
   *
   * See Program::absoluteJumpToAddressIfVariableGt0 for the intended operator use.
   *
   * Perform an absolute jump to a fixed address if a condition variable is > 0.
   *
   * @param condition_variable The variable holding the value to compare
   * @param follow_condition_links Whether to resolve the comparison variable's links
   * @param addr The target address
   */
  void absoluteJumpToAddressIfVariableGt0(
      int32_t condition_variable, bool follow_condition_links, int32_t addr);

  /**
   * @fn VmSession::absoluteJumpToAddressIfVariableLt0
   * @brief NEEDS DOCUMENTATION
   *
   * See Program::absoluteJumpToAddressIfVariableLt0 for the intended operator use.
   *
   * Perform an absolute jump to a fixed address if a condition variable is < 0.
   *
   * @param condition_variable The variable holding the value to compare
   * @param follow_condition_links Whether to resolve the comparison variable's links
   * @param addr The target address
   */
  void absoluteJumpToAddressIfVariableLt0(
      int32_t condition_variable, bool follow_condition_links, int32_t addr);

  /**
   * @fn VmSession::absoluteJumpToAddressIfVariableEq0
   * @brief NEEDS DOCUMENTATION
   *
   * See Program::absoluteJumpToAddressIfVariableEq0 for the intended operator use.
   *
   * Perform an absolute jump to a fixed address if a condition variable is = 0.
   *
   * @param condition_variable The variable holding the value to compare
   * @param follow_condition_links Whether to resolve the comparison variable's links
   * @param addr The target address
   */
  void absoluteJumpToAddressIfVariableEq0(
      int32_t condition_variable, bool follow_condition_links, int32_t addr);

  /**
   * @fn VmSession::loadMemorySizeIntoVariable
   * @brief Loads the maximum number of variables into a variable
   *
   * See Program::loadMemorySizeIntoVariable for the intended operator use.
   *
   * @param variable The variable to store the result in
   * @param follow_links Whether to resolve the variable's links
   */
  void loadMemorySizeIntoVariable(int32_t variable, bool follow_links);

  /**
   * @fn VmSession::checkIfVariableIsInput
   * @brief Checks if a variable is registered as input, stores the result
   *
   * See Program::checkIfVariableIsInput for the intended operator use.
   *
   * Checks whether the variable `source_variable` is registered as an input.
   * If yes, stores `0x1` in `destination_variable`, and `0x0` otherwise.
   *
   * @param source_variable The variable to check for being an input
   * @param follow_source_links Whether to resolve the source variable's links
   * @param destination_variable The variable to store the result in
   * @param follow_destination_links Whether to resolve the destination
   *        variable's links
   */
  void checkIfVariableIsInput(
      int32_t source_variable, bool follow_source_links,
      int32_t destination_variable, bool follow_destination_links);

  /**
   * @fn VmSession::checkIfVariableIsOutput
   * @brief Checks if a variable is registered as output, stores the result
   *
   * See Program::checkIfVariableIsOutput for the intended operator use.
   *
   * Checks whether the variable `source_variable` is registered as an output.
   * If yes, stores `0x1` in `destination_variable`, and `0x0` otherwise.
   *
   * @param source_variable The variable to check for being an output
   * @param follow_source_links Whether to resolve the source variable's links
   * @param destination_variable The variable to store the result in
   * @param follow_destination_links Whether to resolve the destination
   *        variable's links
   */
  void checkIfVariableIsOutput(
      int32_t source_variable, bool follow_source_links,
      int32_t destination_variable, bool follow_destination_links);

  /**
   * @fn VmSession::copyVariable
   * @brief Copies the value of one variable into another
   *
   * See Program::copyVariable for the intended operator use.
   *
   * @param source_variable The variable to copy the value from
   * @param follow_source_links Whether to resolve the source variable's links
   * @param destination_variable The variable to store the result in
   * @param follow_destination_links Whether to resolve the destination
   *        variable's links
   */
  void copyVariable(
      int32_t source_variable, bool follow_source_links,
      int32_t destination_variable, bool follow_destination_links);

  /**
   * @fn VmSession::loadInputCountIntoVariable
   * @brief Loads the number of declared input variables into a variable
   *
   * See Program::loadInputCountIntoVariable for the intended operator use.
   *
   * @param variable The index of the target variable
   * @param follow_links Whether to resolve the variable's links
   */
  void loadInputCountIntoVariable(int32_t variable, bool follow_links);

  /**
   * @fn VmSession::loadOutputCountIntoVariable
   * @brief Loads the number of declared output variables into a variable
   *
   * See Program::loadOutputCountIntoVariable for the intended operator use.
   *
   * @param variable The index of the target variable
   * @param follow_links Whether to resolve the variable's links
   */
  void loadOutputCountIntoVariable(int32_t variable, bool follow_links);

  /**
   * @fn VmSession::loadCurrentAddressIntoVariable
   * @brief Loads the current execution pointer address into a variable
   *
   * See Program::loadCurrentAddressIntoVariable for the intended operator use.
   *
   * @param variable The index of the target variable
   * @param follow_links Whether to resolve the variable's links
   */
  void loadCurrentAddressIntoVariable(int32_t variable, bool follow_links);

  /**
   * @fn VmSession::checkIfInputWasSet
   * @brief Checks if an input variable has received data
   *
   * See Program::checkIfInputWasSet for the intended operator use.
   *
   * If the target variable is not declared as an input variable this call will
   * throw an exception.
   *
   * @param variable_index The index of the input variable to check
   * @param follow_links Whether to resolve the variable's links
   */
  void checkIfInputWasSet(
      int32_t variable_index, bool follow_links,
      int32_t destination_variable, bool follow_destination_links);

  /**
   * @fn VmSession::loadStringTableLimitIntoVariable
   * @brief Loads the maximum number of indices in the string table into a variable
   *
   * See Program::loadStringTableLimitIntoVariable for the intended operator use.
   *
   * @param variable_index The index of the variable
   * @param follow_links Whether to resolve the variable's links
   */
  void loadStringTableLimitIntoVariable(int32_t variable_index, bool follow_links);

  /**
   * @fn VmSession::loadStringTableItemLengthLimitIntoVariable
   * @brief Loads the maximum number of characters per string table item into a variable
   *
   * See Program::loadStringTableItemLengthLimitIntoVariable for the intended operator use.
   *
   * @param variable_index The index of the variable
   * @param follow_links Whether to resolve the variable's links
   */
  void loadStringTableItemLengthLimitIntoVariable(int32_t variable_index, bool follow_links);

  /**
   * @fn VmSession::loadRandomValueIntoVariable
   * @brief Loads a random number in the range of `int32_t` into a variable
   *
   * See Program::loadRandomValueIntoVariable for the intended operator use.
   *
   * @param variable_index The index of the variable
   * @param follow_links Whether to resolve the variable's links
   */
  void loadRandomValueIntoVariable(int32_t variable_index, bool follow_links);

  /**
   * @fn VmSession::unconditionalJumpToAbsoluteAddress
   * @brief Performs an unconditional jump to an absolute address
   *
   * See Program::unconditionalJumpToAbsoluteAddress for the intended operator use.
   *
   * @param addr The absolute address to jump to
   */
  void unconditionalJumpToAbsoluteAddress(int32_t addr);

  /**
   * @fn VmSession::unconditionalJumpToAbsoluteVariableAddress
   * @brief Performs an unconditional jump to an absolute address read from a variable
   *
   * See Program::unconditionalJumpToAbsoluteVariableAddress for the intended operator use.
   *
   * @param variable_index The variable containing the absolute address to jump to
   * @param follow_links Whether to resolve variable links
   */
  void unconditionalJumpToAbsoluteVariableAddress(int32_t variable_index, bool follow_links);

  /**
   * @fn VmSession::unconditionalJumpToRelativeAddress
   * @brief Performs an unconditional jump to a relative address
   *
   * See Program::unconditionalJumpToRelativeAddress for the intended operator use.
   *
   * @param addr The absolute address to jump to
   */
  void unconditionalJumpToRelativeAddress(int32_t addr);

  /**
   * @fn VmSession::unconditionalJumpToRelativeVariableAddress
   * @brief Performs an unconditional jump to a relative address read from a variable
   *
   * See Program::unconditionalJumpToRelativeVariableAddress for the intended operator use.
   *
   * @param variable_index The variable containing the relative address to jump to
   * @param follow_links Whether to resolve variable links
   */
  void unconditionalJumpToRelativeVariableAddress(int32_t variable_index, bool follow_links);

  /**
   * @fn VmSession::loadStringItemLengthIntoVariable
   * @brief Loads the length of a specific string table item into a variable
   *
   * See Program::loadStringItemLengthIntoVariable for the intended operator use.
   *
   * @param string_table_index The string table index to read the length from
   * @param variable_index The variable the length should be stored in
   * @param follow_links Whether to resolve variable links
   */
  void loadStringItemLengthIntoVariable(
      int32_t string_table_index, int32_t variable_index, bool follow_links);

  /**
   * @fn VmSession::loadStringItemIntoVariables
   * @brief Loads a string table item's content into a number of consecutive variables
   *
   * See Program::loadStringItemIntoVariables for the intended operator use.
   *
   * The string item read from the string table will be iterated character by character.
   * Each character goes into a variable at the respective relative index, starting from
   * the specified start variable index. The variables will then contain the respective
   * characters in their least significant byte.
   *
   * Example:
   * * String table item = "Entry"
   * * start_variable_index + 0 = "E"
   * * start_variable_index + 1 = "n"
   * * start_variable_index + 2 = "t"
   * * start_variable_index + 3 = "r"
   * * start_variable_index + 4 = "y"
   *
   * Variable links will be resolved per variable if `follow_links` is `true`.
   *
   * @param string_table_index The string table index to read the characters from
   * @param start_variable_index The variable where the first character is stored
   * @param follow_links Whether to resolve variable links
   * @sa loadVariableStringItemIntoVariables()
   */
  void loadStringItemIntoVariables(
      int32_t string_table_index, int32_t start_variable_index, bool follow_links);

  /**
   * @fn VmSession::performSystemCall
   * @brief Performs actions interacting with the external system
   *
   * System calls are everything categorized as interacting with the external system, i.e. not
   * inside the running program but on VM host level. These functions are rather limited at the
   * moment and don't allow access to sensitive information outside, nor are they allowed to alter
   * any states outside of the program. In their current state, system calls are limited to
   * acquiring environment information such as details about the current date and time. The major
   * and minor code identify the concrete action that will take place, and the result of that
   * selected action will be stored in the variable referenced during the call.
   *
   * Here is a list of currently implemented system calls (major code, minor code, description):
   * * 0, 0: Get UTC timezone (hours)
   * * 0, 1: Get UTC timezone (minutes)
   * * 0, 2: Get current UTC time (seconds part)
   * * 0, 3: Get current UTC time (minutes part)
   * * 0, 4: Get current UTC time (hours part)
   * * 0, 5: Get current UTC date (day part)
   * * 0, 6: Get current UTC date (month part)
   * * 0, 7: Get current UTC date (year part)
   * * 0, 8: Get current UTC date (week part)
   * * 0, 9: Get current UTC date (day of week part)
   *
   * @param major_code The major code for the system call (see table)
   * @param minor_code The minor code for the system call (see table)
   * @param variable_index The variable to store the call's result in
   * @param follow_links Whether to resolve the variable's links
   */
  void performSystemCall(
      int8_t major_code, int8_t minor_code, int32_t variable_index, bool follow_links);

  /**
   * @fn VmSession::bitShiftVariable
   * @brief Bit-shift a variable by a given number of places
   *
   * See Program::bitShiftVariable for the intended operator use.
   *
   * Positive amounts of places shift to the left, negative amounts shift to the right.
   *
   * @param variable_index The variable to bit-shift
   * @param follow_links Whether to resolve variable links
   * @param places The amount of places to shift by
   */
  void bitShiftVariable(int32_t variable_index, bool follow_links, int8_t places);

  /**
   * @fn VmSession::bitWiseInvertVariable
   * @brief Bit-wise invert a variable
   *
   * See Program::bitWiseInvertVariable for the intended operator use.
   *
   * @param variable_index The variable to invert
   * @param follow_links Whether to resolve variable links
   */
  void bitWiseInvertVariable(int32_t variable_index, bool follow_links);

  /**
   * @fn VmSession::bitWiseAndTwoVariables
   * @brief Perform a bit-wise AND operation on two variables
   *
   * See Program::bitWiseAndTwoVariables for the intended operator use.
   *
   * The result will be stored in `variable_index_b`:
   * `b = a & b`
   *
   * @param variable_index The first variable
   * @param follow_links Whether to resolve the first variable's links
   * @param variable_index The second variable (also the target)
   * @param follow_links Whether to resolve the second variable's links
   */
  void bitWiseAndTwoVariables(
      int32_t variable_index_a, bool follow_links_a, int32_t variable_index_b, bool follow_links_b);

  /**
   * @fn VmSession::bitWiseOrTwoVariables
   * @brief Perform a bit-wise OR operation on two variables
   *
   * See Program::bitWiseOrTwoVariables for the intended operator use.
   *
   * The result will be stored in `variable_index_b`:
   * `b = a | b`
   *
   * @param variable_index The first variable
   * @param follow_links Whether to resolve the first variable's links
   * @param variable_index The second variable (also the target)
   * @param follow_links Whether to resolve the second variable's links
   */
  void bitWiseOrTwoVariables(
      int32_t variable_index_a, bool follow_links_a, int32_t variable_index_b, bool follow_links_b);

  /**
   * @fn VmSession::bitWiseXorTwoVariables
   * @brief Perform a bit-wise XOR operation on two variables
   *
   * See Program::bitWiseXorTwoVariables for the intended operator use.
   *
   * The result will be stored in `variable_index_b`:
   * `b = a ^ b`
   *
   * @param variable_index The first variable
   * @param follow_links Whether to resolve the first variable's links
   * @param variable_index The second variable (also the target)
   * @param follow_links Whether to resolve the second variable's links
   */
  void bitWiseXorTwoVariables(
      int32_t variable_index_a, bool follow_links_a, int32_t variable_index_b, bool follow_links_b);

  /**
   * @fn VmSession::moduloVariableByConstant
   * @brief Performs a modulo operation on the variable, using the specified constant
   *
   * See Program::moduloVariableByConstant for the intended operator use.
   *
   * The result will be stored in the same variable.
   * `variable = variable % constant`
   *
   * @param variable_index The variable to modulo
   * @param follow_links Whether to resolve variable links
   * @param constant The constant to modulo the variable's value by
   */
  void moduloVariableByConstant(int32_t variable_index, bool follow_links, int32_t constant);

  /**
   * @fn VmSession::moduloVariableByVariable
   * @brief Performs a modulo operation on a variable, using a modulo value from a second variable
   *
   * See Program::moduloVariableByVariable for the intended operator use.
   *
   * The result will be stored in the first variable.
   * `variable = variable % modulo_variable`
   *
   * @param variable_index The variable to modulo
   * @param follow_links Whether to resolve the variable's links
   * @param modulo_variable_index The variable to use as modulo value
   * @param modulo_follow_links Whether to resolve the modulo variable's links
   */
  void moduloVariableByVariable(
      int32_t variable_index, bool follow_links,
      int32_t modulo_variable_index, bool modulo_follow_links);

  /**
   * @fn VmSession::rotateVariable
   * @brief Bit-wise rotates a variable by a number of places
   *
   * See Program::rotateVariable for the intended operator use.
   *
   * Positive values of `places` rotate to the left, negative values rotate to the right.
   *
   * @param variable_index The variable to rotate
   * @param follow_links Whether to resolve the variable's links
   * @param places The amount of places by which to rotate the variable
   */
  void rotateVariable(int32_t variable_index, bool follow_links, int8_t places);

  /**
   * @fn VmSession::pushVariableOnStack
   * @brief Pushes the value of a variable onto a stack
   *
   * See Program::pushVariableOnStack for the intended operator use.
   *
   * @param stack_variable_index The stack variable index
   * @param stack_follow_links Whether to resolve the stack variable's links
   * @param variable_index The variable whose value to push onto the stack
   * @param follow_links Whether to resolve the value variable's links
   */
  void pushVariableOnStack(
      int32_t stack_variable_index, bool stack_follow_links,
      int32_t variable_index, bool follow_links);

  /**
   * @fn VmSession::pushConstantOnStack
   * @brief Pushes a constant value onto a stack
   *
   * See Program::pushConstantOnStack for the intended operator use.
   *
   * @param stack_variable_index The stack variable index
   * @param stack_follow_links Whether to resolve the stack variable's links
   * @param constant The constant value to push onto the stack
   */
  void pushConstantOnStack(int32_t stack_variable_index, bool stack_follow_links, int32_t constant);

  /**
   * @fn VmSession::popVariableFromStack
   * @brief Pops the top item from a stack and stores its value in a variable
   *
   * See Program::popVariableFromStack for the intended operator use.
   *
   * @param stack_variable_index The stack variable index
   * @param stack_follow_links Whether to resolve the stack variable's links
   * @param variable_index The variable in which to store the top stack item
   * @param follow_links Whether to resolve the target variable's links
   */
  void popVariableFromStack(
      int32_t stack_variable_index, bool stack_follow_links,
      int32_t variable_index, bool follow_links);

  /**
   * @fn VmSession::popTopItemFromStack
   * @brief Removes the top item from a stack and discards it
   *
   * See Program::popTopItemFromStack for the intended operator use.
   *
   * @param stack_variable_index The stack variable index
   * @param stack_follow_links Whether to resolve the stack variable's links
   */
  void popTopItemFromStack(int32_t stack_variable_index, bool stack_follow_links);

  /**
   * @fn VmSession::checkIfStackIsEmpty
   * @brief Determines whether a stack is empty, stores the result
   *
   * See Program::checkIfStackIsEmpty for the intended operator use.
   *
   * If the stack at `stack_variable_index` is empty, the value `0x1` is stored in the target
   * variable. Otherwise, `0x0` is stored.
   *
   * @param stack_variable_index The stack variable index
   * @param stack_follow_links Whether to resolve the stack variable's links
   * @param variable_index The variable in which to store the top stack item
   * @param follow_links Whether to resolve the target variable's links
   */
  void checkIfStackIsEmpty(
      int32_t stack_variable_index, bool stack_follow_links,
      int32_t variable_index, bool follow_links);

  /**
   * @fn VmSession::swapVariables
   * @brief Swaps the content of two variables
   *
   * See Program::swapVariables for the intended operator use.
   *
   * @param variable_index_a The first variable to swap
   * @param follow_links_a Whether to resolve the first variable's links
   * @param variable_index_b The second variable to swap
   * @param follow_links_b Whether to resolve the second variable's links
   */
  void swapVariables(
      int32_t variable_index_a, bool follow_links_a, int32_t variable_index_b, bool follow_links_b);

  /**
   * @fn VmSession::setVariableStringTableEntry
   * @brief Sets the content of a string table item at a variable index
   *
   * See Program::setVariableStringTableEntry for the intended operator use.
   *
   * The index at which the target string is stored in the string table is defined by the
   * variable `variable_index`.
   *
   * @param variable_index The variable containing the string table index
   * @param follow_links Whether to resolve the variable's links
   * @param string_content The string to store in the string table
   */
  void setVariableStringTableEntry(
      int32_t variable_index, bool follow_links, std::string_view string_content);

  /**
   * @fn VmSession::printVariableStringFromStringTable
   * @brief Prints a string from the string table based on a variable index
   *
   * See Program::printVariableStringFromStringTable for the intended operator use.
   *
   * The string value stored in the string table at the defined index
   * `string_table_index` is appended to the print buffer.
   *
   * @param variable_index The variable index to use as string table index
   * @param follow_links Whether to resolve the variable's links
   */
  void printVariableStringFromStringTable(int32_t variable_index, bool follow_links);

  /**
   * @fn VmSession::loadVariableStringItemLengthIntoVariable
   * @brief Loads the length of a variable string table item into a variable
   *
   * See Program::loadVariableStringItemLengthIntoVariable for the intended operator use.
   *
   * Retrieves the length of a stored string table item and stored it in the target variable
   * `variable_index`. The string table item index used is retrieved from
   * `string_item_variable_index`.
   *
   * @param string_item_variable_index The variable holding the string table item index
   * @param string_item_follow_links Whether to resolve the string item variable's links
   * @param variable_index The variable to store the length in
   * @param follow_links Whether to resolve the target variable's links
   * @sa loadStringItemLengthIntoVariable()
   */
  void loadVariableStringItemLengthIntoVariable(
      int32_t string_item_variable_index, bool string_item_follow_links,
      int32_t variable_index, bool follow_links);

  /**
   * @fn VmSession::loadVariableStringItemIntoVariables
   * @brief Loads a variable string table item's content into a number of consecutive variables
   *
   * See Program::loadVariableStringItemIntoVariables for the intended operator use.
   *
   * This call is similar to loadStringItemIntoVariables with the difference that the string
   * table index is not passed as a fixed value, but is read from the variable index
   * `string_item_variable_index`.
   *
   * @param string_item_variable_index The variable holding the string table item index
   * @param string_item_follow_links Whether to resolve the string table index variable's links
   * @param start_variable_index The variable where the first character is stored
   * @param follow_links Whether to resolve the start variable's links
   * @sa loadStringItemIntoVariables()
   */
  void loadVariableStringItemIntoVariables(
      int32_t string_item_variable_index, bool string_item_follow_links,
      int32_t start_variable_index, bool follow_links);

  /**
   * @fn VmSession::terminateWithVariableReturnCode
   * @brief Marks the program as terminates and sets a variable return code
   *
   * See Program::terminateWithVariableReturnCode for the intended operator use.
   *
   * This call is very similar to the terminate call. Instead of setting the return value to
   * a fixed value though, it reads the return code from the variable index
   * `variable_index`.
   *
   * @param variable_index The variable to read the return code from
   * @param follow_links Whether to resolve the variable's links
   * @sa terminate()
   */
  void terminateWithVariableReturnCode(int32_t variable_index, bool follow_links);

  /**
   * @fn VmSession::variableBitShiftVariableLeft
   * @brief Bit-shift a variable by a variable number of places to the left
   *
   * See Program::variableBitShiftVariableLeft for the intended operator use.
   *
   * @param variable_index The variable to bit-shift to the left
   * @param follow_links Whether to resolve variable links
   * @param places_variable_index The variable holding the places to shift by
   * @param places_follow_links Whether to resolve the places variable's links
   * @sa bitShiftVariableLeft()
   */
  void variableBitShiftVariableLeft(
      int32_t variable_index, bool follow_links,
      int32_t places_variable_index, bool places_follow_links);

  /**
   * @fn VmSession::variableBitShiftVariableRight
   * @brief Bit-shift a variable by a variable number of places to the right
   *
   * See Program::variableBitShiftVariableRight for the intended operator use.
   *
   * @param variable_index The variable to bit-shift to the right
   * @param follow_links Whether to resolve variable links
   * @param places_variable_index The variable holding the places to shift by
   * @param places_follow_links Whether to resolve the places variable's links
   * @sa bitShiftVariableRight()
   */
  void variableBitShiftVariableRight(
      int32_t variable_index, bool follow_links,
      int32_t places_variable_index, bool places_follow_links);

  /**
   * @fn VmSession::variableRotateVariableLeft
   * @brief Rotates a variable to the left by a variable amount of places
   *
   * See Program::variableRotateVariableLeft for the intended operator use.
   *
   * @param variable_index The variable to rotate to the left
   * @param follow_links Whether to resolve the rotated variable's links
   * @param places_variable_index The variable holding the amount of places
   *        to rotate
   * @param places_follow_links Whether to resolve the places variable's links
   * @sa rotateVariableLeft(), rotateVariableRight(), variableRotateVariableRight()
   */
  void variableRotateVariableLeft(
      int32_t variable_index, bool follow_links,
      int32_t places_variable_index, bool places_follow_links);

  /**
   * @fn VmSession::variableRotateVariableRight
   * @brief Rotates a variable to the right by a variable amount of places
   *
   * See Program::variableRotateVariableRight for the intended operator use.
   *
   * @param variable_index The variable to rotate to the right
   * @param follow_links Whether to resolve the rotated variable's links
   * @param places_variable_index The variable holding the amount of places
   *        to rotate
   * @param places_follow_links Whether to resolve the places variable's links
   * @sa rotateVariableLeft(), rotateVariableRight(), variableRotateVariableLeft()
   */
  void variableRotateVariableRight(
      int32_t variable_index, bool follow_links,
      int32_t places_variable_index, bool places_follow_links);

  /**
   * @fn VmSession::compareIfVariableGtConstant
   * @brief Checks if a variable is larger than a constant, stores the result
   *
   * See Program::compareIfVariableGtConstant for the intended operator use.
   *
   * If the variable larger than the constant, store `0x1` in the target variable.
   * Store `0x0` otherwise.
   *
   * @param variable_index The variable to compare
   * @param follow_links Whether to resolve the variable's links
   * @param constant The constant to compare to
   * @param target_variable_index The variable to store the result in
   * @param target_follow_links Whether to resolve the target variable's links
   */
  void compareIfVariableGtConstant(
      int32_t variable_index, bool follow_links, int32_t constant,
      int32_t target_variable_index, bool target_follow_links);

  /**
   * @fn VmSession::compareIfVariableLtConstant
   * @brief Checks if a variable is smaller than a constant, stores the result
   *
   * See Program::compareIfVariableLtConstant for the intended operator use.
   *
   * If the variable smaller than the constant, store `0x1` in the target variable.
   * Store `0x0` otherwise.
   *
   * @param variable_index The variable to compare
   * @param follow_links Whether to resolve the variable's links
   * @param constant The constant to compare to
   * @param target_variable_index The variable to store the result in
   * @param target_follow_links Whether to resolve the target variable's links
   */
  void compareIfVariableLtConstant(
      int32_t variable_index, bool follow_links, int32_t constant,
      int32_t target_variable_index, bool target_follow_links);

  /**
   * @fn VmSession::compareIfVariableEqConstant
   * @brief Checks if a variable has the same value as a constant, stores the result
   *
   * See Program::compareIfVariableEqConstant for the intended operator use.
   *
   * If the variable has the same value as the constant, store `0x1` in the target variable.
   * Store `0x0` otherwise.
   *
   * @param variable_index The variable to compare
   * @param follow_links Whether to resolve the variable's links
   * @param constant The constant to compare to
   * @param target_variable_index The variable to store the result in
   * @param target_follow_links Whether to resolve the target variable's links
   */
  void compareIfVariableEqConstant(
      int32_t variable_index, bool follow_links, int32_t constant,
      int32_t target_variable_index, bool target_follow_links);

  /**
   * @fn VmSession::compareIfVariableGtVariable
   * @brief ecks if one variable is larger than another, stores the result
   *
   * See Program::compareIfVariableGtVariable for the intended operator use.
   *
   * If the first variable is larger than the second, store `0x1` in the target variable.
   * Store `0x0` otherwise.
   *
   * @param variable_index_a The first variable to compare
   * @param follow_links_a Whether to resolve the first variable's links
   * @param variable_index_b The second variable to compare
   * @param follow_links_b Whether to resolve the second variable's links
   * @param target_variable_index The variable to store the result in
   * @param target_follow_links Whether to resolve the target variable's links
   */
  void compareIfVariableGtVariable(
      int32_t variable_index_a, bool follow_links_a, int32_t variable_index_b, bool follow_links_b,
      int32_t target_variable_index, bool target_follow_links);

  /**
   * @fn VmSession::compareIfVariableLtVariable
   * @brief Checks if one variable is smaller than another, stores the result
   *
   * See Program::compareIfVariableLtVariable for the intended operator use.
   *
   * If the first variable is smaller than the second, store `0x1` in the target variable.
   * Store `0x0` otherwise.
   *
   * @param variable_index_a The first variable to compare
   * @param follow_links_a Whether to resolve the first variable's links
   * @param variable_index_b The second variable to compare
   * @param follow_links_b Whether to resolve the second variable's links
   * @param target_variable_index The variable to store the result in
   * @param target_follow_links Whether to resolve the target variable's links
   */
  void compareIfVariableLtVariable(
      int32_t variable_index_a, bool follow_links_a, int32_t variable_index_b, bool follow_links_b,
      int32_t target_variable_index, bool target_follow_links);

  /**
   * @fn VmSession::compareIfVariableEqVariable
   * @brief Checks if two variables have the same value, stores the result
   *
   * See Program::compareIfVariableEqVariable for the intended operator use.
   *
   * If the two variable values are equal, store `0x1` in the target variable. Store `0x0` otherwise.
   *
   * @param variable_index_a The first variable to compare
   * @param follow_links_a Whether to resolve the first variable's links
   * @param variable_index_b The second variable to compare
   * @param follow_links_b Whether to resolve the second variable's links
   * @param target_variable_index The variable to store the result in
   * @param target_follow_links Whether to resolve the target variable's links
   */
  void compareIfVariableEqVariable(
      int32_t variable_index_a, bool follow_links_a, int32_t variable_index_b, bool follow_links_b,
      int32_t target_variable_index, bool target_follow_links);

  /**
   * @fn VmSession::getMaxOfVariableAndConstant
   * @brief Chooses the larger of a variable and a constant value, stores the result
   *
   * See Program::getMaxOfVariableAndConstant for the intended operator use.
   *
   * @param variable_index The variable to compare
   * @param follow_links Whether to resolve the variable's links
   * @param constant The constant to compare
   * @param target_variable_index The variable to store the result in
   * @param target_follow_links Whether to resolve the target variable's links
   */
  void getMaxOfVariableAndConstant(
      int32_t variable_index, bool follow_links, int32_t constant,
      int32_t target_variable_index, bool target_follow_links);

  /**
   * @fn VmSession::getMinOfVariableAndConstant
   * @brief Chooses the smaller of a variable and a constant value, stores the result
   *
   * See Program::getMinOfVariableAndConstant for the intended operator use.
   *
   * @param variable_index The variable to compare
   * @param follow_links Whether to resolve the variable's links
   * @param constant The constant to compare
   * @param target_variable_index The variable to store the result in
   * @param target_follow_links Whether to resolve the target variable's links
   */
  void getMinOfVariableAndConstant(
      int32_t variable_index, bool follow_links, int32_t constant,
      int32_t target_variable_index, bool target_follow_links);

  /**
   * @fn VmSession::getMaxOfVariableAndVariable
   * @brief Chooses the larger of two variable values, stores the result
   *
   * See Program::getMaxOfVariableAndVariable for the intended operator use.
   *
   * @param variable_index_a The first variable to compare
   * @param follow_links_a Whether to resolve the first variable's links
   * @param variable_index_b The second variable to compare
   * @param follow_links_b Whether to resolve the second variable's links
   * @param target_variable_index The variable to store the result in
   * @param target_follow_links Whether to resolve the target variable's links
   */
  void getMaxOfVariableAndVariable(
      int32_t variable_index_a, bool follow_links_a, int32_t variable_index_b, bool follow_links_b,
      int32_t target_variable_index, bool target_follow_links);

  /**
   * @fn VmSession::getMinOfVariableAndVariable
   * @brief Chooses the smaller of two variable values, stores the result
   *
   * See Program::getMinOfVariableAndVariable for the intended operator use.
   *
   * @param variable_index_a The first variable to compare
   * @param follow_links_a Whether to resolve the first variable's links
   * @param variable_index_b The second variable to compare
   * @param follow_links_b Whether to resolve the second variable's links
   * @param target_variable_index The variable to store the result in
   * @param target_follow_links Whether to resolve the target variable's links
   */
  void getMinOfVariableAndVariable(
      int32_t variable_index_a, bool follow_links_a, int32_t variable_index_b, bool follow_links_b,
      int32_t target_variable_index, bool target_follow_links);

  /**
   * @fn VmSession::printVariable
   * @brief Prints the value of a variable
   *
   * See Program::printVariable for the intended operator use.
   *
   * If `as_char` is `false`, this appends the numeric value stored at `variable_index` as string
   * to the print buffer. If `as_char` is `true`, only the least significant byte of the variable's
   * value is appended as one character to the print buffer.
   *
   * @param variable_index The variable to read the value from
   * @param follow_links Whether to resolve the variable's links
   * @param as_char Whether to print the numeric or the char value
   */
  void printVariable(int32_t variable_index, bool follow_links, bool as_char);

  /**
   * @fn VmSession::printStringFromStringTable
   * @brief Prints a string from the string table based on a fixed index
   *
   * See Program::printStringFromStringTable for the intended operator use.
   *
   * The string value stored in the string table at the defined index
   * `string_table_index` is appended to the print buffer.
   *
   * @param string_table_index The string table index to read the string from
   */
  void printStringFromStringTable(int32_t string_table_index);

 private:
  /**
   * @fn VmSession::getVariableValueInternal
   * @brief Get and returns the value of a variable and adjusts the read flag
   *
   * @param variable_index The variable to get the value from
   * @param follow_links Whether to resolve the variable's links
   */
  int32_t getVariableValueInternal(int32_t variable_index, bool follow_links);

  /**
   * @fn VmSession::setVariableValueInternal
   * @brief Sets the value of a variable and adjusts the modified flag
   *
   * @param variable_index The variable to set
   * @param follow_links Whether to resolve the variable's links
   * @param value The value to store in the variable
   */
  void setVariableValueInternal(int32_t variable_index, bool follow_links, int32_t value);

  /**
   * @var VmSession::program_
   * @brief The program to execute
   */
  Program program_;

  /**
   * @var VmSession::pointer_
   * @brief The current execution pointer
   *
   * Always points to the byte in the program's byte code that will be executed next.
   */
  int32_t pointer_ = 0;

  /**
   * @var VmSession::variable_count_
   * @brief The maximum number of variables to store in the variable memory
   */
  size_t variable_count_;

  /**
   * @var VmSession::string_table_count_
   * @brief The maximum number of items to store in the string table
   */
  size_t string_table_count_;

  /**
   * @var VmSession::max_string_size_
   * @brief The maximum length of a string stored in the string table
   */
  size_t max_string_size_;

  /**
   * @var VmSession::maximum_print_buffer_length_
   * @brief The maximum length of the print buffer
   *
   * To prevent this buffer from overflowing, clearPrintBuffer should be called regularly.
   *
   * @sa clearPrintBuffer()
   */
  size_t maximum_print_buffer_length_ = 256;

  /**
   * @var VmSession::variables_
   * @brief Holds the program's variable memory
   *
   * The map's index is the actual variables' indices. The content is a pair of the variable's
   * description (type, I/O behavior, modified flag) and the actual numerical value.
   */
  std::map<int32_t, std::pair<VariableDescriptor, int32_t>> variables_;

  /**
   * @var VmSession::string_table_
   * @brief Holds the program's string table
   *
   * The map's index is the actual string table items' indices. The value for each is the actual
   * string value stored for each string table item.
   */
  std::map<int32_t, std::string> string_table_;

  /**
   * @var VmSession::print_buffer_
   * @brief Holds characters to output to screen
   *
   * Any output generated by the program that is supposed to be printed to screen are stored here.
   */
  std::string print_buffer_;

  /**
   * @var VmSession::was_terminated_
   * @brief Denotes whether the program has been terminated.
   *
   * When this flag is set, VirtualMachine instances are not supposed to further execute the
   * program.
   */
  bool was_terminated_ = false;

  /**
   * @var VmSession::return_code_
   * @brief Holds the program's current return code
   *
   * Can be set via terminate calls. Can be retrieved via the getReturnCode function.
   *
   * @sa terminate(), terminateWithVariableReturnCode(), getReturnCode()
   */
  int8_t return_code_ = 0;
};

}  // namespace beast

#endif  // BEAST_VM_SESSION_HPP_
