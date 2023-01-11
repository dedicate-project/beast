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
 * @brief NEEDS DOCUMENTATION
 *
 * TODO(fairlight1337): Document this part.
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
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  VmSession(
      Program program, size_t variable_count, size_t string_table_count,
      size_t max_string_size);

  /**
   * @fn VmSession::setVariableBehavior
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void setVariableBehavior(int32_t variable_index, VariableIoBehavior behavior);

  /**
   * @fn VmSession::getVariableBehavior
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  VariableIoBehavior getVariableBehavior(int32_t variable_index, bool follow_links);

  /**
   * @fn VmSession::getData4
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  int32_t getData4();

  /**
   * @fn VmSession::getData2
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  int16_t getData2();

  /**
   * @fn VmSession::getData1
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  int8_t getData1();

  /**
   * @fn VmSession::getVariableValue
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  int32_t getVariableValue(int32_t variable_index, bool follow_links);

  /**
   * @fn VmSession::setVariableValue
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void setVariableValue(int32_t variable_index, bool follow_links, int32_t value);

  /**
   * @fn VmSession::isAtEnd
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  bool isAtEnd() const;

  /**
   * @fn VmSession::registerVariable
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void registerVariable(int32_t variable_index, Program::VariableType variable_type);

  /**
   * @fn VmSession::getRealVariableIndex
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  int32_t getRealVariableIndex(int32_t variable_index, bool follow_links);

  /**
   * @fn VmSession::setVariable
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void setVariable(int32_t variable_index, int32_t value, bool follow_links);

  /**
   * @fn VmSession::unregisterVariable
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void unregisterVariable(int32_t variable_index);

  /**
   * @fn VmSession::setStringTableEntry
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void setStringTableEntry(int32_t string_table_index, const std::string& string_content);

  /**
   * @fn VmSession::getStringTableEntry
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  const std::string& getStringTableEntry(int32_t string_table_index) const;

  /**
   * @fn VmSession::appendToPrintBuffer
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void appendToPrintBuffer(const std::string& string);

  /**
   * @fn VmSession::appendVariableToPrintBuffer
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void appendVariableToPrintBuffer(int32_t variable_index, bool follow_links, bool as_char);

  /**
   * @fn VmSession::getPrintBuffer
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  const std::string& getPrintBuffer() const;

  /**
   * @fn VmSession::clearPrintBuffer
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void clearPrintBuffer();

  /**
   * @fn VmSession::terminate
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void terminate(int8_t return_code);

  /**
   * @fn VmSession::getReturnCode
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  int8_t getReturnCode() const;

  /**
   * @fn VmSession::addConstantToVariable
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void addConstantToVariable(int32_t variable_index, int32_t constant, bool follow_links);

  /**
   * @fn VmSession::addVariableToVariable
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void addVariableToVariable(int32_t source_variable, int32_t destination_variable, bool follow_source_links, bool follow_destination_links);

  /**
   * @fn VmSession::subtractConstantFromVariable
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void subtractConstantFromVariable(int32_t variable_index, int32_t constant, bool follow_links);

  /**
   * @fn VmSession::subtractVariableFromVariable
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void subtractVariableFromVariable(int32_t source_variable, int32_t destination_variable, bool follow_source_links, bool follow_destination_links);

  /**
   * @fn VmSession::relativeJumpToVariableAddressIfVariableGt0
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void relativeJumpToVariableAddressIfVariableGt0(int32_t condition_variable, bool follow_condition_links, int32_t addr_variable, bool follow_addr_links);

  /**
   * @fn VmSession::relativeJumpToVariableAddressIfVariableLt0
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void relativeJumpToVariableAddressIfVariableLt0(int32_t condition_variable, bool follow_condition_links, int32_t addr_variable, bool follow_addr_links);

  /**
   * @fn VmSession::relativeJumpToVariableAddressIfVariableEq0
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void relativeJumpToVariableAddressIfVariableEq0(int32_t condition_variable, bool follow_condition_links, int32_t addr_variable, bool follow_addr_links);

  /**
   * @fn VmSession::absoluteJumpToVariableAddressIfVariableGt0
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void absoluteJumpToVariableAddressIfVariableGt0(int32_t condition_variable, bool follow_condition_links, int32_t addr_variable, bool follow_addr_links);

  /**
   * @fn VmSession::absoluteJumpToVariableAddressIfVariableLt0
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void absoluteJumpToVariableAddressIfVariableLt0(int32_t condition_variable, bool follow_condition_links, int32_t addr_variable, bool follow_addr_links);

  /**
   * @fn VmSession::absoluteJumpToVariableAddressIfVariableEq0
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void absoluteJumpToVariableAddressIfVariableEq0(int32_t condition_variable, bool follow_condition_links, int32_t addr_variable, bool follow_addr_links);

  /**
   * @fn VmSession::relativeJumpToAddressIfVariableGt0
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void relativeJumpToAddressIfVariableGt0(int32_t condition_variable, bool follow_condition_links, int32_t addr);

  /**
   * @fn VmSession::relativeJumpToAddressIfVariableLt0
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void relativeJumpToAddressIfVariableLt0(int32_t condition_variable, bool follow_condition_links, int32_t addr);

  /**
   * @fn VmSession::relativeJumpToAddressIfVariableEq0
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void relativeJumpToAddressIfVariableEq0(int32_t condition_variable, bool follow_condition_links, int32_t addr);

  /**
   * @fn VmSession::absoluteJumpToAddressIfVariableGt0
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void absoluteJumpToAddressIfVariableGt0(int32_t condition_variable, bool follow_condition_links, int32_t addr);

  /**
   * @fn VmSession::absoluteJumpToAddressIfVariableLt0
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void absoluteJumpToAddressIfVariableLt0(int32_t condition_variable, bool follow_condition_links, int32_t addr);

  /**
   * @fn VmSession::absoluteJumpToAddressIfVariableEq0
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void absoluteJumpToAddressIfVariableEq0(int32_t condition_variable, bool follow_condition_links, int32_t addr);

  /**
   * @fn VmSession::loadMemorySizeIntoVariable
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void loadMemorySizeIntoVariable(int32_t variable, bool follow_links);

  /**
   * @fn VmSession::checkIfVariableIsInput
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void checkIfVariableIsInput(int32_t source_variable, bool follow_source_links, int32_t destination_variable, bool follow_destination_links);

  /**
   * @fn VmSession::checkIfVariableIsOutput
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void checkIfVariableIsOutput(int32_t source_variable, bool follow_source_links, int32_t destination_variable, bool follow_destination_links);

  /**
   * @fn VmSession::copyVariable
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void copyVariable(int32_t source_variable, bool follow_source_links, int32_t destination_variable, bool follow_destination_links);

  /**
   * @fn VmSession::loadInputCountIntoVariable
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void loadInputCountIntoVariable(int32_t variable, bool follow_links);

  /**
   * @fn VmSession::loadOutputCountIntoVariable
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void loadOutputCountIntoVariable(int32_t variable, bool follow_links);

  /**
   * @fn VmSession::loadCurrentAddressIntoVariable
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void loadCurrentAddressIntoVariable(int32_t variable, bool follow_links);

  /**
   * @fn VmSession::checkIfInputWasSet
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void checkIfInputWasSet(int32_t variable_index, bool follow_links, int32_t destination_variable, bool follow_destination_links);

  /**
   * @fn VmSession::loadStringTableLimitIntoVariable
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void loadStringTableLimitIntoVariable(int32_t variable_index, bool follow_links);

  /**
   * @fn VmSession::loadStringTableItemLengthLimitIntoVariable
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void loadStringTableItemLengthLimitIntoVariable(int32_t variable_index, bool follow_links);

  /**
   * @fn VmSession::loadRandomValueIntoVariable
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void loadRandomValueIntoVariable(int32_t variable_index, bool follow_links);

  /**
   * @fn VmSession::unconditionalJumpToAbsoluteAddress
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void unconditionalJumpToAbsoluteAddress(int32_t addr);

  /**
   * @fn VmSession::unconditionalJumpToAbsoluteVariableAddress
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void unconditionalJumpToAbsoluteVariableAddress(int32_t variable_index, bool follow_links);

  /**
   * @fn VmSession::unconditionalJumpToRelativeAddress
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void unconditionalJumpToRelativeAddress(int32_t addr);

  /**
   * @fn VmSession::unconditionalJumpToRelativeVariableAddress
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void unconditionalJumpToRelativeVariableAddress(int32_t variable_index, bool follow_links);

  /**
   * @fn VmSession::loadStringItemLengthIntoVariable
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void loadStringItemLengthIntoVariable(int32_t string_table_index, int32_t variable_index, bool follow_links);

  /**
   * @fn VmSession::loadStringItemIntoVariables
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void loadStringItemIntoVariables(int32_t string_table_index, int32_t start_variable_index, bool follow_links);

  /**
   * @fn VmSession::performSystemCall
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void performSystemCall(int8_t major_code, int8_t minor_code, int32_t variable_index, bool follow_links);

  /**
   * @fn VmSession::bitShiftVariable
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void bitShiftVariable(int32_t variable_index, bool follow_links, int8_t places);

  /**
   * @fn VmSession::bitWiseInvertVariable
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void bitWiseInvertVariable(int32_t variable_index, bool follow_links);

  /**
   * @fn VmSession::bitWiseAndTwoVariables
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void bitWiseAndTwoVariables(int32_t variable_index_a, bool follow_links_a, int32_t variable_index_b, bool follow_links_b);

  /**
   * @fn VmSession::bitWiseOrTwoVariables
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void bitWiseOrTwoVariables(int32_t variable_index_a, bool follow_links_a, int32_t variable_index_b, bool follow_links_b);

  /**
   * @fn VmSession::bitWiseXorTwoVariables
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void bitWiseXorTwoVariables(int32_t variable_index_a, bool follow_links_a, int32_t variable_index_b, bool follow_links_b);

  /**
   * @fn VmSession::moduloVariableByConstant
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void moduloVariableByConstant(int32_t variable_index, bool follow_links, int32_t constant);

  /**
   * @fn VmSession::moduloVariableByVariable
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void moduloVariableByVariable(int32_t variable_index, bool follow_links, int32_t modulo_variable_index, bool modulo_follow_links);

  /**
   * @fn VmSession::rotateVariable
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void rotateVariable(int32_t variable_index, bool follow_links, int8_t places);

  /**
   * @fn VmSession::pushVariableOnStack
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void pushVariableOnStack(int32_t stack_variable_index, bool stack_follow_links, int32_t variable_index, bool follow_links);

  /**
   * @fn VmSession::pushConstantOnStack
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void pushConstantOnStack(int32_t stack_variable_index, bool stack_follow_links, int32_t constant);

  /**
   * @fn VmSession::popVariableFromStack
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void popVariableFromStack(int32_t stack_variable_index, bool stack_follow_links, int32_t variable_index, bool follow_links);

  /**
   * @fn VmSession::popFromStack
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void popFromStack(int32_t stack_variable_index, bool stack_follow_links);

  /**
   * @fn VmSession::checkIfStackIsEmpty
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void checkIfStackIsEmpty(int32_t stack_variable_index, bool stack_follow_links, int32_t variable_index, bool follow_links);

  /**
   * @fn VmSession::swapVariables
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void swapVariables(int32_t variable_index_a, bool follow_links_a, int32_t variable_index_b, bool follow_links_b);

  /**
   * @fn VmSession::setVariableStringTableEntry
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void setVariableStringTableEntry(int32_t variable_index, bool follow_links, const std::string& string_content);

  /**
   * @fn VmSession::printVariableStringFromStringTable
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void printVariableStringFromStringTable(int32_t variable_index, bool follow_links);

  /**
   * @fn VmSession::loadVariableStringItemLengthIntoVariable
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void loadVariableStringItemLengthIntoVariable(int32_t string_item_variable_index, bool string_item_follow_links, int32_t variable_index, bool follow_links);

  /**
   * @fn VmSession::loadVariableStringItemIntoVariables
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void loadVariableStringItemIntoVariables(int32_t string_item_variable_index, bool string_item_follow_links, int32_t start_variable_index, bool follow_links);

  /**
   * @fn VmSession::terminateWithVariableReturnCode
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void terminateWithVariableReturnCode(int32_t variable_index, bool follow_links);

  /**
   * @fn VmSession::variableBitShiftVariableLeft
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void variableBitShiftVariableLeft(int32_t variable_index, bool follow_links, int32_t places_variable_index, bool places_follow_links);

  /**
   * @fn VmSession::variableBitShiftVariableRight
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void variableBitShiftVariableRight(int32_t variable_index, bool follow_links, int32_t places_variable_index, bool places_follow_links);

  /**
   * @fn VmSession::variableRotateVariableLeft
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void variableRotateVariableLeft(int32_t variable_index, bool follow_links, int32_t places_variable_index, bool places_follow_links);

  /**
   * @fn VmSession::variableRotateVariableRight
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void variableRotateVariableRight(int32_t variable_index, bool follow_links, int32_t places_variable_index, bool places_follow_links);
  
  /**
   * @fn VmSession::compareIfVariableGtConstant
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void compareIfVariableGtConstant(int32_t variable_index, bool follow_links, int32_t constant, int32_t target_variable_index, bool target_follow_links);
  
  /**
   * @fn VmSession::compareIfVariableLtConstant
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void compareIfVariableLtConstant(int32_t variable_index, bool follow_links, int32_t constant, int32_t target_variable_index, bool target_follow_links);

  /**
   * @fn VmSession::compareIfVariableEqConstant
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void compareIfVariableEqConstant(int32_t variable_index, bool follow_links, int32_t constant, int32_t target_variable_index, bool target_follow_links);
  
  /**
   * @fn VmSession::compareIfVariableGtVariable
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void compareIfVariableGtVariable(int32_t variable_index_a, bool follow_links_a, int32_t variable_index_b, bool follow_links_b, int32_t target_variable_index, bool target_follow_links);
  
  /**
   * @fn VmSession::compareIfVariableLtVariable
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void compareIfVariableLtVariable(int32_t variable_index_a, bool follow_links_a, int32_t variable_index_b, bool follow_links_b, int32_t target_variable_index, bool target_follow_links);
  
  /**
   * @fn VmSession::compareIfVariableEqVariable
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void compareIfVariableEqVariable(int32_t variable_index_a, bool follow_links_a, int32_t variable_index_b, bool follow_links_b, int32_t target_variable_index, bool target_follow_links);

  /**
   * @fn VmSession::getMaxOfVariableAndConstant
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void getMaxOfVariableAndConstant(int32_t variable_index, bool follow_links, int32_t constant, int32_t target_variable_index, bool target_follow_links);

  /**
   * @fn VmSession::getMinOfVariableAndConstant
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void getMinOfVariableAndConstant(int32_t variable_index, bool follow_links, int32_t constant, int32_t target_variable_index, bool target_follow_links);

  /**
   * @fn VmSession::getMaxOfVariableAndVariable
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void getMaxOfVariableAndVariable(int32_t variable_index_a, bool follow_links_a, int32_t variable_index_b, bool follow_links_b, int32_t target_variable_index, bool target_follow_links);

  /**
   * @fn VmSession::getMinOfVariableAndVariable
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void getMinOfVariableAndVariable(int32_t variable_index_a, bool follow_links_a, int32_t variable_index_b, bool follow_links_b, int32_t target_variable_index, bool target_follow_links);

 private:
  /**
   * @fn VmSession::getVariableValueInternal
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  int32_t getVariableValueInternal(int32_t variable_index, bool follow_links);

  /**
   * @fn VmSession::setVariableValueInternal
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void setVariableValueInternal(int32_t variable_index, bool follow_links, int32_t value);

  /**
   * @var VmSession::program_
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  Program program_;

  /**
   * @var VmSession::pointer_
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  int32_t pointer_;

  /**
   * @var VmSession::variable_count_
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  size_t variable_count_;

  /**
   * @var VmSession::string_table_count_
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  size_t string_table_count_;

  /**
   * @var VmSession::max_string_size_
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  size_t max_string_size_;

  /**
   * @var VmSession::variables_
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  std::map<int32_t, std::pair<VariableDescriptor, int32_t>> variables_;

  /**
   * @var VmSession::string_table_
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  std::map<int32_t, std::string> string_table_;

  /**
   * @var VmSession::print_buffer_
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  std::string print_buffer_;

  /**
   * @var VmSession::was_terminated_
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  bool was_terminated_;

  /**
   * @var VmSession::return_code_
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  int8_t return_code_;
};

}  // namespace beast

#endif  // BEAST_VM_SESSION_HPP_
