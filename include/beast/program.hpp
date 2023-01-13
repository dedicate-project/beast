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
   * @fn Program::Program(std::vector<unsigned char>)
   * @brief Predefined bytecode program constructor
   *
   * This constructor initializes the program with a predefined bytecode vector. The program is
   * defined as constant size, with the size limit being set to the size of the passed in bytecode
   * vector.
   *
   * @param data The bytecode data to initialize the program with.
   */
  Program(std::vector<unsigned char> data);

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
   * @brief Declares a variable at an index and assigns a type
   *
   * Declares a numerical variable index, alongside its designated type. A variable type can either
   * be an actual value storage (of type VariableType::Int32) or a link to another variable index
   * (of type VariableType::Link).
   *
   * Identified by OpCode::DeclareVariable. Represented by 6 bytes.
   *
   * @param variable_index The index to declare the variable at.
   * @param variable_type The designated type of the variable.
   * @sa undeclareVariable()
   */
  void declareVariable(int32_t variable_index, VariableType variable_type);

  /**
   * @fn Program::setVariable
   * @brief Sets the value of a variable
   *
   * Sets the value of a variable at the given index to the value `content`. The variable index
   * needs to be declared prior to this.
   *
   * Identified by OpCode::SetVariable. Represented by 10 bytes.
   *
   * @param variable_index The index of the variable.
   * @param content The value to assign to the variable.
   * @param follow_links Whether to resolve variable links.
   * @sa declareVariable()
   */
  void setVariable(int32_t variable_index, int32_t content, bool follow_links);

  /**
   * @fn Program::undeclareVariable
   * @brief Removes a variable declaration at the given index
   *
   * The variable declared at the given index is undeclared. The index, type, and value are
   * removed from the variable memory. If at that index no variable is declared, an exception
   * is thrown.
   *
   * Identified by OpCode::UndeclareVariable. Represented by 5 bytes.
   *
   * @param variable_index The index of the variable to undeclare.
   * @sa declareVariable()
   */
  void undeclareVariable(int32_t variable_index);

  /**
   * @fn Program::addConstantToVariable
   * @brief Adds a constant value to the value held in a variable
   *
   * Identified by OpCode::AddConstantToVariable. Represented by 8 bytes.
   *
   * @param variable_index The index of the variable to add the constant to.
   * @param constant The constant value to add to the variable value.
   * @param follow_links Whether to resolve variable links.
   */
  void addConstantToVariable(int32_t variable_index, int32_t constant, bool follow_links);

  /**
   * @fn Program::addVariableToVariable
   * @brief Adds the value of one variable to that of another variable
   *
   * The contents of `source_variable_index` are added to the contents of
   * `destination_variable_index`. The result is stored in `destination_variable_index`.
   *
   * Identified by OpCode::AddVariableToVariable. Represented by 11 bytes.
   *
   * @param source_variable_index The index of the source variable.
   * @param follow_source_links Whether to resolve source variable links.
   * @param destination_variable_index The index of the destination variable.
   * @param follow_destination_links Whether to resolve destination variable links.
   */
  void addVariableToVariable(
    int32_t source_variable_index, bool follow_source_links,
    int32_t destination_variable_index, bool follow_destination_links);

  /**
   * @fn Program::subtractConstantFromVariable
   * @brief Subtract a constant value from the value held in a variable
   *
   * Identified by OpCode::SubtractConstantFromVariable. Represented by 10 bytes.
   *
   * @param variable_index The index of the variable to subtract the constant from.
   * @param constant The constant value to subtract from the variable value.
   * @param follow_links Whether to resolve variable links.
   */
  void subtractConstantFromVariable(int32_t variable_index, int32_t constant, bool follow_links);

  /**
   * @fn Program::subtractVariableFromVariable
   * @brief Subtracts the value of one variable from that of another variable
   *
   * The contents of `source_variable_index` are subtracted from the contents of
   * `destination_variable_index`. The result is stored in `destination_variable_index`.
   *
   * Identified by OpCode::SubtractVariableFromVariable. Represented by 11 bytes.
   *
   * @param source_variable_index The index of the variable.
   * @param follow_source_links Whether to resolve variable links.
   * @param destination_variable_index The index of the variable.
   * @param follow_destination_links Whether to resolve variable links.
   */
  void subtractVariableFromVariable(
    int32_t source_variable_index, bool follow_source_links,
    int32_t destination_variable_index, bool follow_destination_links);

  /**
   * @fn Program::relativeJumpToVariableAddressIfVariableGreaterThanZero
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   *
   * Identified by OpCode::RelativeJumpToVariableAddressIfVariableGt0. Represented by 11 bytes.
   *
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   * @param relative_jump_address_variable_index tbd
   * @param follow_addr_links tbd
   */
  void relativeJumpToVariableAddressIfVariableGreaterThanZero(
    int32_t variable_index, bool follow_links,
    int32_t relative_jump_address_variable_index, bool follow_addr_links);

  /**
   * @fn Program::relativeJumpToVariableAddressIfVariableLessThanZero
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   *
   * Identified by OpCode::RelativeJumpToVariableAddressIfVariableLt0. Represented by 11 bytes.
   *
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   * @param relative_jump_address_variable_index tbd
   * @param follow_addr_links tbd
   */
  void relativeJumpToVariableAddressIfVariableLessThanZero(
    int32_t variable_index, bool follow_links,
    int32_t relative_jump_address_variable_index, bool follow_addr_links);

  /**
   * @fn Program::relativeJumpToVariableAddressIfVariableEqualsZero
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   *
   * Identified by OpCode::RelativeJumpToVariableAddressIfVariableEq0. Represented by 11 bytes.
   *
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   * @param relative_jump_address_variable_index tbd
   * @param follow_addr_links tbd
   */
  void relativeJumpToVariableAddressIfVariableEqualsZero(
    int32_t variable_index, bool follow_links,
    int32_t relative_jump_address_variable_index, bool follow_addr_links);

  /**
   * @fn Program::absoluteJumpToVariableAddressIfVariableGreaterThanZero
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   *
   * Identified by OpCode::AbsoluteJumpToVariableAddressIfVariableGt0. Represented by 11 bytes.
   *
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   * @param absolute_jump_address_variable_index tbd
   * @param follow_addr_links tbd
   */
  void absoluteJumpToVariableAddressIfVariableGreaterThanZero(
    int32_t variable_index, bool follow_links,
    int32_t absolute_jump_address_variable_index, bool follow_addr_links);

  /**
   * @fn Program::absoluteJumpToVariableAddressIfVariableLessThanZero
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   *
   * Identified by OpCode::AbsoluteJumpToVariableAddressIfVariableLt0. Represented by 11 bytes.
   *
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   * @param absolute_jump_address_variable_index tbd
   * @param follow_addr_links tbd
   */
  void absoluteJumpToVariableAddressIfVariableLessThanZero(
    int32_t variable_index, bool follow_links,
    int32_t absolute_jump_address_variable_index, bool follow_addr_links);

  /**
   * @fn Program::absoluteJumpToVariableAddressIfVariableEqualsZero
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   *
   * Identified by OpCode::AbsoluteJumpToVariableAddressIfVariableEq0. Represented by 11 bytes.
   *
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   * @param absolute_jump_address_variable_index tbd
   * @param follow_addr_links tbd
   */
  void absoluteJumpToVariableAddressIfVariableEqualsZero(
    int32_t variable_index, bool follow_links,
    int32_t absolute_jump_address_variable_index, bool follow_addr_links);

  /**
   * @fn Program::relativeJumpToAddressIfVariableGreaterThanZero
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   *
   * Identified by OpCode::RelativeJumpIfVariableGt0. Represented by 10 bytes.
   *
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   * @param relative_jump_address tbd
   */
  void relativeJumpToAddressIfVariableGreaterThanZero(
    int32_t variable_index, bool follow_links, int32_t relative_jump_address);

  /**
   * @fn Program::relativeJumpToAddressIfVariableLessThanZero
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   *
   * Identified by OpCode::RelativeJumpIfVariableLt0. Represented by 10 bytes.
   *
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   * @param relative_jump_address tbd
   */
  void relativeJumpToAddressIfVariableLessThanZero(
    int32_t variable_index, bool follow_links, int32_t relative_jump_address);

  /**
   * @fn Program::relativeJumpToAddressIfVariableEqualsZero
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   *
   * Identified by OpCode::RelativeJumpIfVariableEq0. Represented by 10 bytes.
   *
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   * @param relative_jump_address tbd
   */
  void relativeJumpToAddressIfVariableEqualsZero(
    int32_t variable_index, bool follow_links, int32_t relative_jump_address);

  /**
   * @fn Program::absoluteJumpToAddressIfVariableGreaterThanZero
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   *
   * Identified by OpCode::AbsoluteJumpIfVariableGt0. Represented by 10 bytes.
   *
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   * @param absolute_jump_address tbd
   */
  void absoluteJumpToAddressIfVariableGreaterThanZero(
    int32_t variable_index, bool follow_links, int32_t absolute_jump_address);

  /**
   * @fn Program::absoluteJumpToAddressIfVariableLessThanZero
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   *
   * Identified by OpCode::AbsoluteJumpIfVariableLt0. Represented by 10 bytes.
   *
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   * @param absolute_jump_address tbd
   */
  void absoluteJumpToAddressIfVariableLessThanZero(
    int32_t variable_index, bool follow_links, int32_t absolute_jump_address);

  /**
   * @fn Program::absoluteJumpToAddressIfVariableEqualsZero
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   *
   * Identified by OpCode::AbsoluteJumpIfVariableEq0. Represented by 10 bytes.
   *
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   * @param absolute_jump_address tbd
   */
  void absoluteJumpToAddressIfVariableEqualsZero(
    int32_t variable_index, bool follow_links, int32_t absolute_jump_address);

  /**
   * @fn Program::loadMemorySizeIntoVariable
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   *
   * Identified by OpCode::LoadMemorySizeIntoVariable. Represented by 6 bytes.
   *
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   */
  void loadMemorySizeIntoVariable(int32_t variable_index, bool follow_links);

  /**
   * @fn Program::checkIfVariableIsInput
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   *
   * Identified by OpCode::CheckIfVariableIsInput. Represented by 11 bytes.
   *
   * @param source_variable_index The index of the source variable.
   * @param follow_source_links Whether to resolve source variable links.
   * @param destination_variable_index The index of the destination variable.
   * @param follow_destination_links Whether to resolve destination variable links.
   */
  void checkIfVariableIsInput(
    int32_t source_variable_index, bool follow_source_links,
    int32_t destination_variable_index, bool follow_destination_links);

  /**
   * @fn Program::checkIfVariableIsOutput
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   *
   * Identified by OpCode::CheckIfVariableIsOutput. Represented by 11 bytes.
   *
   * @param source_variable_index The index of the source variable.
   * @param follow_source_links Whether to resolve source variable links.
   * @param destination_variable_index The index of the destination variable.
   * @param follow_destination_links Whether to resolve destination variable links.
   */
  void checkIfVariableIsOutput(
    int32_t source_variable_index, bool follow_source_links,
    int32_t destination_variable_index, bool follow_destination_links);

  /**
   * @fn Program::loadInputCountIntoVariable
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   *
   * Identified by OpCode::LoadInputCountIntoVariable. Represented by 6 bytes.
   *
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   */
  void loadInputCountIntoVariable(int32_t variable_index, bool follow_links);

  /**
   * @fn Program::loadOutputCountIntoVariable
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   *
   * Identified by OpCode::LoadOutputCountIntoVariable. Represented by 6 bytes.
   *
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   */
  void loadOutputCountIntoVariable(int32_t variable_index, bool follow_links);

  /**
   * @fn Program::loadCurrentAddressIntoVariable
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   *
   * Identified by OpCode::LoadCurrentAddressIntoVariable. Represented by 6 bytes.
   *
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   */
  void loadCurrentAddressIntoVariable(int32_t variable_index, bool follow_links);

  /**
   * @fn Program::printVariable
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   *
   * Identified by OpCode::PrintVariable. Represented by 7 bytes.
   *
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   * @param as_char tbd
   */
  void printVariable(int32_t variable_index, bool follow_links, bool as_char);

  /**
   * @fn Program::setStringTableEntry
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   *
   * Identified by OpCode::SetStringTableEntry. Represented by 7 + string length bytes.
   *
   * @param string_table_index tbd
   * @param string tbd
   */
  void setStringTableEntry(int32_t string_table_index, const std::string& string);

  /**
   * @fn Program::printStringFromStringTable
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   *
   * Identified by OpCode::PrintStringFromStringTable. Represented by 5 bytes.
   *
   * @param string_table_index tbd
   */
  void printStringFromStringTable(int32_t string_table_index);

  /**
   * @fn Program::loadStringTableLimitIntoVariable
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   *
   * Identified by OpCode::LoadStringTableLimitIntoVariable. Represented by 6 bytes.
   *
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   */
  void loadStringTableLimitIntoVariable(int32_t variable_index, bool follow_links);

  /**
   * @fn Program::terminate
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   *
   * Identified by OpCode::Terminate. Represented by 2 bytes.
   *
   * @param return_code tbd
   */
  void terminate(int8_t return_code);

  /**
   * @fn Program::copyVariable
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   *
   * Identified by OpCode::CopyVariable. Represented by 11 bytes.
   *
   * @param source_variable_index The index of the source variable.
   * @param follow_source_links Whether to resolve source variable links.
   * @param destination_variable_index The index of the destination variable.
   * @param follow_destination_links Whether to resolve destination variable links.
   */
  void copyVariable(
      int32_t source_variable_index, bool follow_source_links,
      int32_t destination_variable_index, bool follow_destination_links);

  /**
   * @fn Program::loadStringItemLengthIntoVariable
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   *
   * Identified by OpCode::LoadStringItemLengthIntoVariable. Represented by 10 bytes.
   *
   * @param string_table_index tbd
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   */
  void loadStringItemLengthIntoVariable(
      int32_t string_table_index, int32_t variable_index, bool follow_links);

  /**
   * @fn Program::checkIfInputWasSet
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   *
   * Identified by OpCode::CheckIfInputWasSet. Represented by 11 bytes.
   *
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   * @param destination_variable_index The index of the destination variable.
   * @param follow_destination_links Whether to resolve destination variable links.
   */
  void checkIfInputWasSet(
      int32_t variable_index, bool follow_links,
      int32_t destination_variable_index, bool follow_destination_links);

  /**
   * @fn Program::loadStringTableItemLengthLimitIntoVariable
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   *
   * Identified by OpCode::LoadStringTableItemLengthLimitIntoVariable. Represented by 6 bytes.
   *
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   */
  void loadStringTableItemLengthLimitIntoVariable(int32_t variable_index, bool follow_links);

  /**
   * @fn Program::loadRandomValueIntoVariable
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   *
   * Identified by OpCode::LoadRandomValueIntoVariable. Represented by 6 bytes.
   *
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   */
  void loadRandomValueIntoVariable(int32_t variable_index, bool follow_links);

  /**
   * @fn Program::unconditionalJumpToAbsoluteAddress
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   *
   * Identified by OpCode::UnconditionalJumpToAbsoluteAddress. Represented by 5 bytes.
   *
   * @param addr tbd
   */
  void unconditionalJumpToAbsoluteAddress(int32_t addr);

  /**
   * @fn Program::unconditionalJumpToAbsoluteVariableAddress
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   *
   * Identified by OpCode::UnconditionalJumpToAbsoluteVariableAddress. Represented by 6 bytes.
   *
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   */
  void unconditionalJumpToAbsoluteVariableAddress(int32_t variable_index, bool follow_links);

  /**
   * @fn Program::unconditionalJumpToRelativeAddress
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   *
   * Identified by OpCode::UnconditionalJumpToRelativeAddress. Represented by 5 bytes.
   *
   * @param addr tbd
   */
  void unconditionalJumpToRelativeAddress(int32_t addr);

  /**
   * @fn Program::unconditionalJumpToRelativeVariableAddress
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   *
   * Identified by OpCode::UnconditionalJumpToRelativeVariableAddress. Represented by 6 bytes.
   *
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   */
  void unconditionalJumpToRelativeVariableAddress(int32_t variable_index, bool follow_links);

  /**
   * @fn Program::loadStringItemIntoVariables
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   *
   * Identified by OpCode::LoadStringItemIntoVariables. Represented by 10 bytes.
   *
   * @param string_table_index tbd
   * @param start_variable_index The index of the start variable.
   * @param follow_links Whether to resolve variable links.
   */
  void loadStringItemIntoVariables(
      int32_t string_table_index, int32_t start_variable_index, bool follow_links);

  /**
   * @fn Program::performSystemCall
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   *
   * Identified by OpCode::PerformSystemCall. Represented by 8 bytes.
   *
   * @param major_code tbd
   * @param minor_code tbd
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   */
  void performSystemCall(
      int8_t major_code, int8_t minor_code, int32_t variable_index, bool follow_links);

  /**
   * @fn Program::bitShiftVariableLeft
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   *
   * Identified by OpCode::BitShiftVariableLeft. Represented by 7 bytes.
   *
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   * @param places tbd
   */
  void bitShiftVariableLeft(int32_t variable_index, bool follow_links, int8_t places);

  /**
   * @fn Program::bitShiftVariableRight
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   *
   * Identified by OpCode::BitShiftVariableRight. Represented by 7 bytes.
   *
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   * @param places tbd
   */
  void bitShiftVariableRight(int32_t variable_index, bool follow_links, int8_t places);

  /**
   * @fn Program::bitWiseInvertVariable
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   *
   * Identified by OpCode::BitWiseInvertVariable. Represented by 5 bytes.
   *
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   */
  void bitWiseInvertVariable(int32_t variable_index, bool follow_links);

  /**
   * @fn Program::bitWiseAndTwoVariables
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   *
   * Identified by OpCode::BitWiseAndTwoVariables. Represented by 11 bytes.
   *
   * @param variable_index_a The index of the variable.
   * @param follow_links_a Whether to resolve variable links.
   * @param variable_index_b The index of the variable.
   * @param follow_links_b Whether to resolve variable links.
   */
  void bitWiseAndTwoVariables(
      int32_t variable_index_a, bool follow_links_a, int32_t variable_index_b, bool follow_links_b);

  /**
   * @fn Program::bitWiseOrTwoVariables
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   *
   * Identified by OpCode::BitWiseOrTwoVariables. Represented by 11 bytes.
   *
   * @param variable_index_a The index of the variable.
   * @param follow_links_a Whether to resolve variable links.
   * @param variable_index_b The index of the variable.
   * @param follow_links_b Whether to resolve variable links.
   */
  void bitWiseOrTwoVariables(
      int32_t variable_index_a, bool follow_links_a, int32_t variable_index_b, bool follow_links_b);

  /**
   * @fn Program::bitWiseXorTwoVariables
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   *
   * Identified by OpCode::BitWiseXorTwoVariables. Represented by 11 bytes.
   *
   * @param variable_index_a The index of the variable.
   * @param follow_links_a Whether to resolve variable links.
   * @param variable_index_b The index of the variable.
   * @param follow_links_b Whether to resolve variable links.
   */
  void bitWiseXorTwoVariables(
      int32_t variable_index_a, bool follow_links_a, int32_t variable_index_b, bool follow_links_b);

  /**
   * @fn Program::moduloVariableByConstant
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   *
   * Identified by OpCode::ModuloVariableByConstant. Represented by 11 bytes.
   *
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   * @param modulo_constant tbd
   */
  void moduloVariableByConstant(
      int32_t variable_index, bool follow_links, int32_t modulo_constant);

  /**
   * @fn Program::moduloVariableByVariable
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   *
   * Identified by OpCode::ModuloVariableByVariable. Represented by 11 bytes.
   *
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   * @param modulo_variable_index The index of the variable.
   * @param modulo_follow_links Whether to resolve variable links.
   */
  void moduloVariableByVariable(
      int32_t variable_index, bool follow_links,
      int32_t modulo_variable_index, bool modulo_follow_links);

  /**
   * @fn Program::rotateVariableLeft
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   *
   * Identified by OpCode::RotateVariableLeft. Represented by 7 bytes.
   *
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   * @param places tbd
   */
  void rotateVariableLeft(int32_t variable_index, bool follow_links, int8_t places);

  /**
   * @fn Program::rotateVariableRight
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   *
   * Identified by OpCode::RotateVariableRight. Represented by 7 bytes.
   *
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   * @param places tbd
   */
  void rotateVariableRight(int32_t variable_index, bool follow_links, int8_t places);

  /**
   * @fn Program::pushVariableOnStack
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   *
   * Identified by OpCode::PushVariableOnStack. Represented by 11 bytes.
   *
   * @param stack_variable_index The index of the variable.
   * @param follow_links_stack Whether to resolve variable links.
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   */
  void pushVariableOnStack(
      int32_t stack_variable_index, bool follow_links_stack,
      int32_t variable_index, bool follow_links);

  /**
   * @fn Program::pushConstantOnStack
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   *
   * Identified by OpCode::PushConstantOnStack. Represented by 10 bytes.
   *
   * @param stack_variable_index The index of the variable.
   * @param follow_links_stack Whether to resolve variable links.
   * @param constant tbd
   */
  void pushConstantOnStack(
      int32_t stack_variable_index, bool follow_links_stack, int32_t constant);

  /**
   * @fn Program::popVariableFromStack
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   *
   * Identified by OpCode::PopVariableFromStack. Represented by 11 bytes.
   *
   * @param stack_variable_index The index of the variable.
   * @param follow_links_stack Whether to resolve variable links.
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   */
  void popVariableFromStack(
      int32_t stack_variable_index, bool follow_links_stack,
      int32_t variable_index, bool follow_links);

  /**
   * @fn Program::popTopItemFromStack
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   *
   * Identified by OpCode::PopTopItemFromStack. Represented by 6 bytes.
   *
   * @param stack_variable_index The index of the variable.
   * @param follow_links_stack Whether to resolve variable links.
   */
  void popTopItemFromStack(int32_t stack_variable_index, bool follow_links_stack);

  /**
   * @fn Program::checkIfStackIsEmpty
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   *
   * Identified by OpCode::CheckIfStackIsEmpty. Represented by 11 bytes.
   *
   * @param stack_variable_index The index of the variable.
   * @param follow_links_stack Whether to resolve variable links.
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   */
  void checkIfStackIsEmpty(
      int32_t stack_variable_index, bool follow_links_stack,
      int32_t variable_index, bool follow_links);

  /**
   * @fn Program::swapVariables
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   *
   * Identified by OpCode::SwapVariables. Represented by 11 bytes.
   *
   * @param variable_index_a The index of the variable.
   * @param follow_links_a Whether to resolve variable links.
   * @param variable_index_b The index of the variable.
   * @param follow_links_b Whether to resolve variable links.
   */
  void swapVariables(
      int32_t variable_index_a, bool follow_links_a, int32_t variable_index_b, bool follow_links_b);

  /**
   * @fn Program::setVariableStringTableEntry
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   *
   * Identified by OpCode::SetVariableStringTableEntry. Represented by 8 + string length bytes.
   *
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   * @param string tbd
   */
  void setVariableStringTableEntry(
      int32_t variable_index, bool follow_links, const std::string& string);

  /**
   * @fn Program::printVariableStringFromStringTable
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   *
   * Identified by OpCode::PrintVariableStringFromStringTable. Represented by 6 bytes.
   *
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   */
  void printVariableStringFromStringTable(int32_t variable_index, bool follow_links);

  /**
   * @fn Program::loadVariableStringItemLengthIntoVariable
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   *
   * Identified by OpCode::LoadVariableStringItemLengthIntoVariable. Represented by 11 bytes.
   *
   * @param string_item_variable_index The index of the variable.
   * @param follow_links_string_item Whether to resolve variable links.
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   */
  void loadVariableStringItemLengthIntoVariable(
      int32_t string_item_variable_index, bool follow_links_string_item,
      int32_t variable_index, bool follow_links);

  /**
   * @fn Program::loadVariableStringItemIntoVariables
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   *
   * Identified by OpCode::LoadVariableStringItemIntoVariables. Represented by 11 bytes.
   *
   * @param string_item_variable_index The index of the variable.
   * @param follow_links_string_item Whether to resolve variable links.
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   */
  void loadVariableStringItemIntoVariables(
      int32_t string_item_variable_index, bool follow_links_string_item,
      int32_t variable_index, bool follow_links);

  /**
   * @fn Program::terminateWithVariableReturnCode
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   *
   * Identified by OpCode::TerminateWithVariableReturnCode. Represented by 6 bytes.
   *
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   */
  void terminateWithVariableReturnCode(int32_t variable_index, bool follow_links);

  /**
   * @fn Program::variableBitShiftVariableLeft
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   *
   * Identified by OpCode::VariableBitShiftVariableLeft. Represented by 11 bytes.
   *
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   * @param places_variable_index The index of the variable.
   * @param follow_links_places Whether to resolve variable links.
   */
  void variableBitShiftVariableLeft(
      int32_t variable_index, bool follow_links,
      int32_t places_variable_index, bool follow_links_places);

  /**
   * @fn Program::variableBitShiftVariableRight
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   *
   * Identified by OpCode::VariableBitShiftVariableRight. Represented by 11 bytes.
   *
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   * @param places_variable_index The index of the variable.
   * @param follow_links_places Whether to resolve variable links.
   */
  void variableBitShiftVariableRight(
      int32_t variable_index, bool follow_links,
      int32_t places_variable_index, bool follow_links_places);

  /**
   * @fn Program::variableRotateVariableLeft
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   *
   * Identified by OpCode::VariableRotateVariableLeft. Represented by 11 bytes.
   *
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   * @param places_variable_index The index of the variable.
   * @param follow_links_places Whether to resolve variable links.
   */
  void variableRotateVariableLeft(
      int32_t variable_index, bool follow_links,
      int32_t places_variable_index, bool follow_links_places);

  /**
   * @fn Program::variableRotateVariableRight
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   *
   * Identified by OpCode::VariableRotateVariableRight. Represented by 11 bytes.
   *
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   * @param places_variable_index The index of the variable.
   * @param follow_links_places Whether to resolve variable links.
   */
  void variableRotateVariableRight(
      int32_t variable_index, bool follow_links,
      int32_t places_variable_index, bool follow_links_places);

  /**
   * @fn Program::compareIfVariableGtConstant
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   *
   * Identified by OpCode::CompareIfVariableGtConstant. Represented by 15 bytes.
   *
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   * @param constant The constant
   * @param target_variable_index The index of the variable.
   * @param target_follow_links Whether to resolve variable links.
   */
  void compareIfVariableGtConstant(
      int32_t variable_index, bool follow_links, int32_t constant,
      int32_t target_variable_index, bool target_follow_links);

  /**
   * @fn Program::compareIfVariableLtConstant
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   *
   * Identified by OpCode::CompareIfVariableLtConstant. Represented by 15 bytes.
   *
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   * @param constant The constant
   * @param target_variable_index The index of the variable.
   * @param target_follow_links Whether to resolve variable links.
   */
  void compareIfVariableLtConstant(
      int32_t variable_index, bool follow_links, int32_t constant,
      int32_t target_variable_index, bool target_follow_links);

  /**
   * @fn Program::compareIfVariableEqConstant
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   *
   * Identified by OpCode::CompareIfVariableEqConstant. Represented by 15 bytes.
   *
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   * @param constant The constant
   * @param target_variable_index The index of the variable.
   * @param target_follow_links Whether to resolve variable links.
   */
  void compareIfVariableEqConstant(
      int32_t variable_index, bool follow_links, int32_t constant,
      int32_t target_variable_index, bool target_follow_links);

  /**
   * @fn Program::compareIfVariableGtVariable
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   *
   * Identified by OpCode::CompareIfVariableGtVariable. Represented by 16 bytes.
   *
   * @param variable_index_a The index of the variable.
   * @param follow_links_a Whether to resolve variable links.
   * @param variable_index_b The index of the variable.
   * @param follow_links_b Whether to resolve variable links.
   * @param target_variable_index The index of the variable.
   * @param target_follow_links Whether to resolve variable links.
   */
  void compareIfVariableGtVariable(
      int32_t variable_index_a, bool follow_links_a, int32_t variable_index_b, bool follow_links_b,
      int32_t target_variable_index, bool target_follow_links);

  /**
   * @fn Program::compareIfVariableLtVariable
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   *
   * Identified by OpCode::CompareIfVariableLtVariable. Represented by 16 bytes.
   *
   * @param variable_index_a The index of the variable.
   * @param follow_links_a Whether to resolve variable links.
   * @param variable_index_b The index of the variable.
   * @param follow_links_b Whether to resolve variable links.
   * @param target_variable_index The index of the variable.
   * @param target_follow_links Whether to resolve variable links.
   */
  void compareIfVariableLtVariable(
      int32_t variable_index_a, bool follow_links_a, int32_t variable_index_b, bool follow_links_b,
      int32_t target_variable_index, bool target_follow_links);

  /**
   * @fn Program::compareIfVariableEqVariable
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this part.
   *
   * Identified by OpCode::CompareIfVariableEqVariable. Represented by 16 bytes.
   *
   * @param variable_index_a The index of the variable.
   * @param follow_links_a Whether to resolve variable links.
   * @param variable_index_b The index of the variable.
   * @param follow_links_b Whether to resolve variable links.
   * @param target_variable_index The index of the variable.
   * @param target_follow_links Whether to resolve variable links.
   */
  void compareIfVariableEqVariable(
      int32_t variable_index_a, bool follow_links_a, int32_t variable_index_b, bool follow_links_b,
      int32_t target_variable_index, bool target_follow_links);

  /**
   * @fn Program::getMaxOfVariableAndConstant
   * @brief Compare a variable to a constant and store the larger value
   *
   * This operator compares the value stored in a variable against a constant. The larger value is
   * stored in the target variable.
   *
   * Identifier by OpCode::GetMaxOfVariableAndConstant. Represented by 15 bytes.
   *
   * @param variable_index The index of the variable used for the comparison.
   * @param follow_links Whether to follow variable links for the comparison variable.
   * @param constant The constant to compare the variable against.
   * @param target_variable_index The index of the variable to store the result in.
   * @param target_follow_links whether to follow variable links for the target variable.
   * @sa getMinOfVariableAndConstant(), getMaxOfVariableAndVariable(), getMinOfVariableAndVariable()
   */
  void getMaxOfVariableAndConstant(
      int32_t variable_index, bool follow_links, int32_t constant,
      int32_t target_variable_index, bool target_follow_links);

  /**
   * @fn Program::getMinOfVariableAndConstant
   * @brief Compare a variable to a constant and store the smaller value
   *
   * This operator compares the value stored in a variable against a constant. The smaller value is
   * stored in the target variable.
   *
   * Identifier by OpCode::GetMinOfVariableAndConstant. Represented by 15 bytes.
   *
   * @param variable_index The index of the variable used for the comparison.
   * @param follow_links Whether to follow variable links for the comparison variable.
   * @param constant The constant to compare the variable against.
   * @param target_variable_index The index of the variable to store the result in.
   * @param target_follow_links whether to follow variable links for the target variable.
   * @sa getMaxOfVariableAndConstant(), getMaxOfVariableAndVariable(), getMinOfVariableAndVariable()
   */
  void getMinOfVariableAndConstant(
      int32_t variable_index, bool follow_links, int32_t constant,
      int32_t target_variable_index, bool target_follow_links);

  /**
   * @fn Program::getMaxOfVariableAndVariable
   * @brief Compare a variable to another variable and store the larger value
   *
   * This operator compares the value stored in one variable against the value stored in another
   * variable. The larger value is stored in the target variable.
   *
   * Identifier by OpCode::GetMaxOfVariableAndVariable. Represented by 16 bytes.
   *
   * @param variable_index_a The index of the first variable used for the comparison.
   * @param follow_links_a Whether to follow the first variable's links for the comparison variable.
   * @param variable_index_b The index of the second variable used for the comparison.
   * @param follow_links_b Whether to follow the second variable's links for the comparison variable.
   * @param target_variable_index The index of the variable to store the result in.
   * @param target_follow_links whether to follow variable links for the target variable.
   * @sa getMaxOfVariableAndConstant(), getMinOfVariableAndConstant(), getMinOfVariableAndVariable()
   */
  void getMaxOfVariableAndVariable(
      int32_t variable_index_a, bool follow_links_a, int32_t variable_index_b, bool follow_links_b,
      int32_t target_variable_index, bool target_follow_links);

  /**
   * @fn Program::getMinOfVariableAndVariable
   * @brief Compare a variable to another variable and store the smaller value
   *
   * This operator compares the value stored in one variable against the value stored in another
   * variable. The smaller value is stored in the target variable.
   *
   * Identifier by OpCode::GetMinOfVariableAndVariable. Represented by 16 bytes.
   *
   * @param variable_index_a The index of the first variable used for the comparison.
   * @param follow_links_a Whether to follow the first variable's links for the comparison variable.
   * @param variable_index_b The index of the second variable used for the comparison.
   * @param follow_links_b Whether to follow the second variable's links for the comparison variable.
   * @param target_variable_index The index of the variable to store the result in.
   * @param target_follow_links whether to follow variable links for the target variable.
   * @sa getMaxOfVariableAndConstant(), getMinOfVariableAndConstant(), getMaxOfVariableAndVariable()
   */
  void getMinOfVariableAndVariable(
      int32_t variable_index_a, bool follow_links_a, int32_t variable_index_b, bool follow_links_b,
      int32_t target_variable_index, bool target_follow_links);

 private:
  /**
   * @fn Program::canFit
   * @brief Checks if a number of bytes fits into the available program space
   *
   * Starting from the current program pointer, this method returns a boolean flag denoting whether
   * a number of bytes can fit into the remaining available program space. For constant size programs,
   * this is limited by the program size it was initialized with. For dynamically growing programs,
   * the available space is expanded upon this call to fit the requested bytes.
   *
   * @param bytes The number of bytes to fit into the program space.
   * 
   * @return True when the bytes fit into the available program space, False otherwise.
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
