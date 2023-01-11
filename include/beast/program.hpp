#ifndef BEAST_PROGRAM_HPP_
#define BEAST_PROGRAM_HPP_

// Standard
#include <cstdint>
#include <string>
#include <vector>

// Internal
#include <beast/opcodes.hpp>

namespace beast {

/**
 * @class Program
 * @brief A class holding the bytecode of a program that can be executed by the BEAST VM.
 *
 * This is the main container class for program code that is executable by the BEAST Virtual Machine
 * (for example using CpuVirtualMachine through a VmSession instance). It allows to add any
 * available operator through a well-defined interface. The program has a constant size mode and a
 * growing mode, depending on whether it was initialized with a maximum size. In constant size mode,
 * no operator code can be added beyond the constructor-defined byte limit. In growing mode, the
 * program size is virtually unlimited (depending on the available host memory). The program class
 * also allows BEAST Virtual Machine instances to read individual parts and get general information
 * about the program.
 *
 * If any error arises during program definition (e.g., operands are inserted beyond a constant size
 * limit), an exception is thrown.
 *
 * @author Jan Winkler
 * @date 2023-01-11
 */
class Program {
 public:
  /**
   * @brief Enumeration of known variable types
   *
   * Variables that can be used by a program must be of one of these defined types. When a variable
   * is declared in a program, the type needs to be passed in. A variable cannot be re-declared with
   * a new type unless it is undeclared first.
   */
  enum class VariableType {
    Int32 = 0,  ///< A four byte signed integer type
    Link = 1    ///< A link to another variable (resolved when the variable content is accessed)
  };

  /**
   * @fn Program::Program
   * @brief Growing program constructor
   *
   * This constructor declares the program as growing dynamically when operators are added to it or
   * other programs are inserted into it.
   */
  Program();

  /**
   * @fn Program::Program(int32_t)
   * @brief Constant size program constructor
   *
   * This constructor declares the program with a constant size in bytes. If more byte code than
   * available in the defined space is added to the program, an exception is thrown. The available
   * space is initialized with `0x0`.
   *
   * @param space The constant space available in the program.
   */
  Program(int32_t space);

  /**
   * @fn Program::getSize
   * @brief Return the current size of the program in bytes
   *
   * For constant size programs this returns the defined maximum size in bytes, for dynamically
   * growing programs it returns the actually used space of the operands added so far.
   *
   * @return The size of the program in bytes.
   */
  size_t getSize() const;

  /**
   * @fn Program::getData4
   * @brief Return the next 4 byte batch starting from an offset
   *
   * Returns the next 4 bytes from the program, starting at the defined offset. The value is
   * returned as a signed int32_t variable. If the end point of the returned region would be beyond
   * the end of the program, an exception is thrown.
   *
   * @param offset The starting point from where to return the data.
   * @return A 4 byte variable containing the next 4 program bytes, starting from an offset.
   * @sa getData2(), getData1()
   */
  int32_t getData4(int32_t offset);

  /**
   * @fn Program::getData2
   * @brief Return the next 2 byte batch starting from an offset
   *
   * Returns the next 2 bytes from the program, starting at the defined offset. The value is
   * returned as a signed int16_t variable. If the end point of the returned region would be beyond
   * the end of the program, an exception is thrown.
   *
   * @param offset The starting point from where to return the data.
   * @return A 2 byte variable containing the next 2 program bytes, starting from an offset.
   * @sa getData4(), getData1()
   */
  int16_t getData2(int32_t offset);

  /**
   * @fn Program::getData1
   * @brief Return the next byte starting from an offset
   *
   * Returns the next byte from the program, starting at the defined offset. The value is returned
   * as a signed int8_t variable. If the end point of the returned region would be beyond the end of
   * the program, an exception is thrown.
   *
   * @param offset The starting point from where to return the data.
   * @return A 1 byte variable containing the next 1 program byte, starting from an offset.
   * @sa getData4(), getData2()
   */
  int8_t getData1(int32_t offset);

  /**
   * @fn Program::getPointer
   * @brief Returns the byte position after the last inserted operator
   *
   * When an operand is added to the program, its code will be places at the location of this
   * pointer. The pointer variable will then be advanced by the amount of bytes inserted. Hence, the
   * pointer returned here always points to the next byte that has not yet been populated. For
   * constant size programs, this points to the byte after the defined available space once the
   * program is fully populated.
   *
   * @return The current program population pointer position.
   */
  uint32_t getPointer() const;

  /**
   * @fn Program::insertProgram
   * @brief Inserts another program into this instance.
   *
   * Any other program instance (constant size or dynamically growing) can be added to a
   * program. The code of the program to be inserted will be added at the current program population
   * pointer position. In the case of constant size programs, if the program to be added is larger
   * than the available space, an exception is thrown. For dynamically growing programs, the
   * containing program will be expanded accordingly.
   *
   * @param other The program to be inserted into the current instance.
   */
  void insertProgram(Program& other);

  /**
   * @fn Program::getData
   * @brief Returns this program's byte code
   *
   * This function returns a constant reference to the actual byte code contained in this program
   * instance. For constant size programs, the returned std::vector will contain the amount of bytes
   * corresponding to the program's initialized maximum size. For dynamically growing programs, the
   * std::vector will be of the exact size of the bytes previously added.
   *
   * @return A constant std::vector containing the byte code of this instance.
   */
  const std::vector<unsigned char>& getData() const;

  /**
   * @fn Program::noop
   * @brief Adds a NoOp operation to the program
   *
   * A NoOp operation performs no operation when executed.
   *
   * Identified by OpCode::NoOp. Represented by 1 byte.
   */
  void noop();

  /**
   * @fn Program::declareVariable
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void declareVariable(int32_t variable_index, VariableType variable_type);

  /**
   * @fn Program::setVariable
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void setVariable(int32_t variable_index, int32_t content, bool follow_links);

  /**
   * @fn Program::undeclareVariable
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void undeclareVariable(int32_t variable_index);

  /**
   * @fn Program::addConstantToVariable
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void addConstantToVariable(int32_t variable_index, int32_t constant, bool follow_links);

  /**
   * @fn Program::addVariableToVariable
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void addVariableToVariable(
    int32_t source_variable_index, bool follow_source_links,
    int32_t destination_variable_index, bool follow_destination_links);

  /**
   * @fn Program::subtractConstantFromVariable
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void subtractConstantFromVariable(int32_t variable_index, int32_t constant, bool follow_links);

  /**
   * @fn Program::subtractVariableFromVariable
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void subtractVariableFromVariable(
    int32_t source_variable_index, bool follow_source_links,
    int32_t destination_variable_index, bool follow_destination_links);

  /**
   * @fn Program::relativeJumpToVariableAddressIfVariableGreaterThanZero
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void relativeJumpToVariableAddressIfVariableGreaterThanZero(
    int32_t variable_index, bool follow_links,
    int32_t relative_jump_address_variable_index, bool follow_addr_links);

  /**
   * @fn Program::relativeJumpToVariableAddressIfVariableLessThanZero
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void relativeJumpToVariableAddressIfVariableLessThanZero(
    int32_t variable_index, bool follow_links,
    int32_t relative_jump_address_variable_index, bool follow_addr_links);

  /**
   * @fn Program::relativeJumpToVariableAddressIfVariableEqualsZero
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void relativeJumpToVariableAddressIfVariableEqualsZero(
    int32_t variable_index, bool follow_links,
    int32_t relative_jump_address_variable_index, bool follow_addr_links);

  /**
   * @fn Program::absoluteJumpToVariableAddressIfVariableGreaterThanZero
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void absoluteJumpToVariableAddressIfVariableGreaterThanZero(
    int32_t variable_index, bool follow_links,
    int32_t absolute_jump_address_variable_index, bool follow_addr_links);

  /**
   * @fn Program::absoluteJumpToVariableAddressIfVariableLessThanZero
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void absoluteJumpToVariableAddressIfVariableLessThanZero(
    int32_t variable_index, bool follow_links,
    int32_t absolute_jump_address_variable_index, bool follow_addr_links);

  /**
   * @fn Program::absoluteJumpToVariableAddressIfVariableEqualsZero
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void absoluteJumpToVariableAddressIfVariableEqualsZero(
    int32_t variable_index, bool follow_links,
    int32_t absolute_jump_address_variable_index, bool follow_addr_links);

  /**
   * @fn Program::relativeJumpToAddressIfVariableGreaterThanZero
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void relativeJumpToAddressIfVariableGreaterThanZero(
    int32_t variable_index, bool follow_links, int32_t relative_jump_address);

  /**
   * @fn Program::relativeJumpToAddressIfVariableLessThanZero
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void relativeJumpToAddressIfVariableLessThanZero(
    int32_t variable_index, bool follow_links, int32_t relative_jump_address);

  /**
   * @fn Program::relativeJumpToAddressIfVariableEqualsZero
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void relativeJumpToAddressIfVariableEqualsZero(
    int32_t variable_index, bool follow_links, int32_t relative_jump_address);

  /**
   * @fn Program::absoluteJumpToAddressIfVariableGreaterThanZero
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void absoluteJumpToAddressIfVariableGreaterThanZero(
    int32_t variable_index, bool follow_links, int32_t absolute_jump_address);

  /**
   * @fn Program::absoluteJumpToAddressIfVariableLessThanZero
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void absoluteJumpToAddressIfVariableLessThanZero(
    int32_t variable_index, bool follow_links, int32_t absolute_jump_address);

  /**
   * @fn Program::absoluteJumpToAddressIfVariableEqualsZero
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void absoluteJumpToAddressIfVariableEqualsZero(
    int32_t variable_index, bool follow_links, int32_t absolute_jump_address);

  /**
   * @fn Program::loadMemorySizeIntoVariable
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void loadMemorySizeIntoVariable(int32_t variable_index, bool follow_links);

  /**
   * @fn Program::checkIfVariableIsInput
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void checkIfVariableIsInput(
    int32_t source_variable_index, bool follow_source_links,
    int32_t destination_variable_index, bool follow_destination_links);

  /**
   * @fn Program::checkIfVariableIsOutput
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void checkIfVariableIsOutput(
    int32_t source_variable_index, bool follow_source_links,
    int32_t destination_variable_index, bool follow_destination_links);

  /**
   * @fn Program::loadInputCountIntoVariable
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void loadInputCountIntoVariable(int32_t variable_index, bool follow_links);

  /**
   * @fn Program::loadOutputCountIntoVariable
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void loadOutputCountIntoVariable(int32_t variable_index, bool follow_links);

  /**
   * @fn Program::loadCurrentAddressIntoVariable
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void loadCurrentAddressIntoVariable(int32_t variable_index, bool follow_links);

  /**
   * @fn Program::printVariable
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void printVariable(int32_t variable_index, bool follow_links, bool as_char);

  /**
   * @fn Program::setStringTableEntry
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void setStringTableEntry(int32_t string_table_index, const std::string& string);

  /**
   * @fn Program::printStringFromStringTable
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void printStringFromStringTable(int32_t string_table_index);

  /**
   * @fn Program::loadStringTableLimitIntoVariable
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void loadStringTableLimitIntoVariable(int32_t variable_index, bool follow_links);

  /**
   * @fn Program::terminate
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void terminate(int8_t return_code);

  /**
   * @fn Program::copyVariable
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void copyVariable(
      int32_t source_variable_index, bool follow_source_links,
      int32_t destination_variable_index, bool follow_destination_links);

  /**
   * @fn Program::loadStringItemLengthIntoVariable
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void loadStringItemLengthIntoVariable(int32_t string_table_index, int32_t variable_index, bool follow_links);

  /**
   * @fn Program::checkIfInputWasSet
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void checkIfInputWasSet(
      int32_t variable_index, bool follow_links,
      int32_t destination_variable_index, bool follow_destination_links);

  /**
   * @fn Program::loadStringTableItemLengthLimitIntoVariable
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void loadStringTableItemLengthLimitIntoVariable(int32_t variable_index, bool follow_links);

  /**
   * @fn Program::loadRandomValueIntoVariable
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void loadRandomValueIntoVariable(int32_t variable_index, bool follow_links);

  /**
   * @fn Program::unconditionalJumpToAbsoluteAddress
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void unconditionalJumpToAbsoluteAddress(int32_t addr);

  /**
   * @fn Program::unconditionalJumpToAbsoluteVariableAddress
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void unconditionalJumpToAbsoluteVariableAddress(int32_t variable_index, bool follow_links);

  /**
   * @fn Program::unconditionalJumpToRelativeAddress
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void unconditionalJumpToRelativeAddress(int32_t addr);

  /**
   * @fn Program::unconditionalJumpToRelativeVariableAddress
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void unconditionalJumpToRelativeVariableAddress(int32_t variable_index, bool follow_links);

  /**
   * @fn Program::loadStringItemIntoVariables
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void loadStringItemIntoVariables(int32_t string_table_index, int32_t start_variable_index, bool follow_links);

  /**
   * @fn Program::performSystemCall
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void performSystemCall(int8_t major_code, int8_t minor_code, int32_t variable_index, bool follow_links);

  /**
   * @fn Program::bitShiftVariableLeft
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void bitShiftVariableLeft(int32_t variable_index, bool follow_links, int8_t places);

  /**
   * @fn Program::bitShiftVariableRight
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void bitShiftVariableRight(int32_t variable_index, bool follow_links, int8_t places);

  /**
   * @fn Program::bitWiseInvertVariable
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void bitWiseInvertVariable(int32_t variable_index, bool follow_links);

  /**
   * @fn Program::bitWiseAndTwoVariables
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void bitWiseAndTwoVariables(int32_t variable_index_a, bool follow_links_a, int32_t variable_index_b, bool follow_links_b);

  /**
   * @fn Program::bitWiseOrTwoVariables
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void bitWiseOrTwoVariables(int32_t variable_index_a, bool follow_links_a, int32_t variable_index_b, bool follow_links_b);

  /**
   * @fn Program::bitWiseXorTwoVariables
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void bitWiseXorTwoVariables(int32_t variable_index_a, bool follow_links_a, int32_t variable_index_b, bool follow_links_b);

  /**
   * @fn Program::moduloVariableByConstant
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void moduloVariableByConstant(int32_t variable_index, bool follow_links, int32_t modulo_constant);

  /**
   * @fn Program::moduloVariableByVariable
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void moduloVariableByVariable(int32_t variable_index, bool follow_links, int32_t modulo_variable_index, bool modulo_follow_links);

  /**
   * @fn Program::rotateVariableLeft
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void rotateVariableLeft(int32_t variable_index, bool follow_links, int8_t places);

  /**
   * @fn Program::rotateVariableRight
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void rotateVariableRight(int32_t variable_index, bool follow_links, int8_t places);

  /**
   * @fn Program::pushVariableOnStack
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void pushVariableOnStack(int32_t stack_variable_index, bool follow_links_stack, int32_t variable_index, bool follow_links);

  /**
   * @fn Program::pushConstantOnStack
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void pushConstantOnStack(int32_t stack_variable_index, bool follow_links_stack, int32_t constant);

  /**
   * @fn Program::popVariableFromStack
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void popVariableFromStack(int32_t stack_variable_index, bool follow_links_stack, int32_t variable_index, bool follow_links);

  /**
   * @fn Program::popTopItemFromStack
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void popTopItemFromStack(int32_t stack_variable_index, bool follow_links_stack);

  /**
   * @fn Program::checkIfStackIsEmpty
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void checkIfStackIsEmpty(int32_t stack_variable_index, bool follow_links_stack, int32_t variable_index, bool follow_links);

  /**
   * @fn Program::swapVariables
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void swapVariables(int32_t variable_index_a, bool follow_links_a, int32_t variable_index_b, bool follow_links_b);

  /**
   * @fn Program::setVariableStringTableEntry
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void setVariableStringTableEntry(int32_t variable_index, bool follow_links, const std::string& string);

  /**
   * @fn Program::printVariableStringFromStringTable
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void printVariableStringFromStringTable(int32_t variable_index, bool follow_links);

  /**
   * @fn Program::loadVariableStringItemLengthIntoVariable
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void loadVariableStringItemLengthIntoVariable(int32_t string_item_variable_index, bool follow_links_string_item, int32_t variable_index, bool follow_links);

  /**
   * @fn Program::loadVariableStringItemIntoVariables
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void loadVariableStringItemIntoVariables(int32_t string_item_variable_index, bool follow_links_string_item, int32_t variable_index, bool follow_links);

  /**
   * @fn Program::terminateWithVariableReturnCode
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void terminateWithVariableReturnCode(int32_t variable_index, bool follow_links);

  /**
   * @fn Program::variableBitShiftVariableLeft
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void variableBitShiftVariableLeft(int32_t variable_index, bool follow_links, int32_t places_variable_index, bool follow_links_places);

  /**
   * @fn Program::variableBitShiftVariableRight
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void variableBitShiftVariableRight(int32_t variable_index, bool follow_links, int32_t places_variable_index, bool follow_links_places);

  /**
   * @fn Program::variableRotateVariableLeft
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void variableRotateVariableLeft(int32_t variable_index, bool follow_links, int32_t places_variable_index, bool follow_links_places);

  /**
   * @fn Program::variableRotateVariableRight
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void variableRotateVariableRight(int32_t variable_index, bool follow_links, int32_t places_variable_index, bool follow_links_places);

 private:
  /**
   * @fn Program::canFit
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  bool canFit(uint32_t bytes);

  /**
   * @fn Program::appendData4
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void appendData4(int32_t data);

  /**
   * @fn Program::appendData2
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void appendData2(int16_t data);

  /**
   * @fn Program::appendData1
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void appendData1(int8_t data);

  /**
   * @fn Program::appendFlag1
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void appendFlag1(bool flag);

  /**
   * @fn Program::appendCode1
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void appendCode1(OpCode opcode);

  /**
   * @fn Program::ensureSize
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  void ensureSize(uint32_t size);

  /**
   * @var Program::data_
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  std::vector<unsigned char> data_;

  /**
   * @var Program::pointer_
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  uint32_t pointer_;

  /**
   * @var Program::grows_dynamically_
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   */
  bool grows_dynamically_;
};

}  // namespace beast

#endif  // BEAST_PROGRAM_HPP_
