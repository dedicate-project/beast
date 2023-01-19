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
  explicit Program(int32_t space);

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
  explicit Program(std::vector<unsigned char> data);

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
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
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
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
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
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
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
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
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
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
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
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
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
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
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
   * @brief Performs a relative jump to a variable address if a variable is > 0
   *
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
   *
   * Identified by OpCode::RelativeJumpToVariableAddressIfVariableGt0. Represented by 11 bytes.
   *
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   * @param relative_jump_address_variable_index The variable containing the absolute jump
   *        address.
   * @param follow_addr_links Whether to follow links in the address variable.
   */
  void relativeJumpToVariableAddressIfVariableGreaterThanZero(
    int32_t variable_index, bool follow_links,
    int32_t relative_jump_address_variable_index, bool follow_addr_links);

  /**
   * @fn Program::relativeJumpToVariableAddressIfVariableLessThanZero
   * @brief Performs a relative jump to a variable address if a variable is < 0
   *
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
   *
   * Identified by OpCode::RelativeJumpToVariableAddressIfVariableLt0. Represented by 11 bytes.
   *
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   * @param relative_jump_address_variable_index The variable containing the absolute jump
   *        address.
   * @param follow_addr_links Whether to follow links in the address variable.
   */
  void relativeJumpToVariableAddressIfVariableLessThanZero(
    int32_t variable_index, bool follow_links,
    int32_t relative_jump_address_variable_index, bool follow_addr_links);

  /**
   * @fn Program::relativeJumpToVariableAddressIfVariableEqualsZero
   * @brief Performs a relative jump to a variable address if a variable is = 0
   *
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
   *
   * Identified by OpCode::RelativeJumpToVariableAddressIfVariableEq0. Represented by 11 bytes.
   *
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   * @param relative_jump_address_variable_index The variable containing the absolute jump
   *        address.
   * @param follow_addr_links Whether to follow links in the address variable.
   */
  void relativeJumpToVariableAddressIfVariableEqualsZero(
    int32_t variable_index, bool follow_links,
    int32_t relative_jump_address_variable_index, bool follow_addr_links);

  /**
   * @fn Program::absoluteJumpToVariableAddressIfVariableGreaterThanZero
   * @brief Performs an absolute jump to a variable address if a variable is > 0
   *
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
   *
   * Identified by OpCode::AbsoluteJumpToVariableAddressIfVariableGt0. Represented by 11 bytes.
   *
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   * @param absolute_jump_address_variable_index The variable containing the absolute jump
   *        address.
   * @param follow_addr_links Whether to follow links in the address variable.
   */
  void absoluteJumpToVariableAddressIfVariableGreaterThanZero(
    int32_t variable_index, bool follow_links,
    int32_t absolute_jump_address_variable_index, bool follow_addr_links);

  /**
   * @fn Program::absoluteJumpToVariableAddressIfVariableLessThanZero
   * @brief Performs an absolute jump to a variable address if a variable is < 0
   *
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
   *
   * Identified by OpCode::AbsoluteJumpToVariableAddressIfVariableLt0. Represented by 11 bytes.
   *
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   * @param absolute_jump_address_variable_index The variable containing the absolute jump
   *        address.
   * @param follow_addr_links Whether to follow links in the address variable.
   */
  void absoluteJumpToVariableAddressIfVariableLessThanZero(
    int32_t variable_index, bool follow_links,
    int32_t absolute_jump_address_variable_index, bool follow_addr_links);

  /**
   * @fn Program::absoluteJumpToVariableAddressIfVariableEqualsZero
   * @brief Performs an absolute jump to a variable address if a variable is = 0
   *
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
   *
   * Identified by OpCode::AbsoluteJumpToVariableAddressIfVariableEq0. Represented by 11 bytes.
   *
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   * @param absolute_jump_address_variable_index The variable containing the absolute jump
   *        address.
   * @param follow_addr_links Whether to follow links in the address variable.
   */
  void absoluteJumpToVariableAddressIfVariableEqualsZero(
    int32_t variable_index, bool follow_links,
    int32_t absolute_jump_address_variable_index, bool follow_addr_links);

  /**
   * @fn Program::relativeJumpToAddressIfVariableGreaterThanZero
   * @brief Performs a relative jump to a fixed address if a variable is > 0
   *
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
   *
   * Identified by OpCode::RelativeJumpIfVariableGt0. Represented by 10 bytes.
   *
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   * @param relative_jump_address The relative address to jump to.
   */
  void relativeJumpToAddressIfVariableGreaterThanZero(
    int32_t variable_index, bool follow_links, int32_t relative_jump_address);

  /**
   * @fn Program::relativeJumpToAddressIfVariableLessThanZero
   * @brief Performs a relative jump to a fixed address if a variable is < 0
   *
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
   *
   * Identified by OpCode::RelativeJumpIfVariableLt0. Represented by 10 bytes.
   *
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   * @param relative_jump_address The relative address to jump to.
   */
  void relativeJumpToAddressIfVariableLessThanZero(
    int32_t variable_index, bool follow_links, int32_t relative_jump_address);

  /**
   * @fn Program::relativeJumpToAddressIfVariableEqualsZero
   * @brief Performs a relative jump to a fixed address if a variable is = 0
   *
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
   *
   * Identified by OpCode::RelativeJumpIfVariableEq0. Represented by 10 bytes.
   *
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   * @param relative_jump_address The relative address to jump to.
   */
  void relativeJumpToAddressIfVariableEqualsZero(
    int32_t variable_index, bool follow_links, int32_t relative_jump_address);

  /**
   * @fn Program::absoluteJumpToAddressIfVariableGreaterThanZero
   * @brief Performs an absolute jump to a fixed address if a variable is > 0
   *
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
   *
   * Identified by OpCode::AbsoluteJumpIfVariableGt0. Represented by 10 bytes.
   *
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   * @param absolute_jump_address The absolute address to jump to.
   */
  void absoluteJumpToAddressIfVariableGreaterThanZero(
    int32_t variable_index, bool follow_links, int32_t absolute_jump_address);

  /**
   * @fn Program::absoluteJumpToAddressIfVariableLessThanZero
   * @brief Performs an absolute jump to a fixed address if a variable is < 0
   *
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
   *
   * Identified by OpCode::AbsoluteJumpIfVariableLt0. Represented by 10 bytes.
   *
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   * @param absolute_jump_address The absolute address to jump to.
   */
  void absoluteJumpToAddressIfVariableLessThanZero(
    int32_t variable_index, bool follow_links, int32_t absolute_jump_address);

  /**
   * @fn Program::absoluteJumpToAddressIfVariableEqualsZero
   * @brief Performs an absolute jump to a fixed address if a variable is = 0
   *
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
   *
   * Identified by OpCode::AbsoluteJumpIfVariableEq0. Represented by 10 bytes.
   *
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   * @param absolute_jump_address The absolute address to jump to.
   */
  void absoluteJumpToAddressIfVariableEqualsZero(
    int32_t variable_index, bool follow_links, int32_t absolute_jump_address);

  /**
   * @fn Program::loadMemorySizeIntoVariable
   * @brief Loads the maximum number of VmSession variables into a variable
   *
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
   *
   * Identified by OpCode::LoadMemorySizeIntoVariable. Represented by 6 bytes.
   *
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   */
  void loadMemorySizeIntoVariable(int32_t variable_index, bool follow_links);

  /**
   * @fn Program::checkIfVariableIsInput
   * @brief Check if a variable is defined as an input
   *
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
   *
   * If the variable `source_variable_index` is defined as having the
   * VariableIoBehavior::Input, set the value of the variable
   * `destination_variable_index` to `0x1`, else set it to `0x0`.
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
   * @brief Check if a variable is defined as an output
   *
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
   *
   * If the variable `source_variable_index` is defined as having the
   * VariableIoBehavior::Output, set the value of the variable
   * `destination_variable_index` to `0x1`, else set it to `0x0`.
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
   * @brief Load the number of registered input variables into a variable
   *
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
   *
   * Identified by OpCode::LoadInputCountIntoVariable. Represented by 6 bytes.
   *
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   */
  void loadInputCountIntoVariable(int32_t variable_index, bool follow_links);

  /**
   * @fn Program::loadOutputCountIntoVariable
   * @brief Load the number of registered output variables into a variable
   *
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
   *
   * Identified by OpCode::LoadOutputCountIntoVariable. Represented by 6 bytes.
   *
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   */
  void loadOutputCountIntoVariable(int32_t variable_index, bool follow_links);

  /**
   * @fn Program::loadCurrentAddressIntoVariable
   * @brief Load the current program execution pointer value into a variable
   *
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
   *
   * Identified by OpCode::LoadCurrentAddressIntoVariable. Represented by 6 bytes.
   *
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   */
  void loadCurrentAddressIntoVariable(int32_t variable_index, bool follow_links);

  /**
   * @fn Program::printVariable
   * @brief Print the value of a variable
   *
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
   *
   * The value stored in the variable `variable_index` will be printed. If
   * `as_char` is `true`, then the variable's least significant byte will be
   * printed as an ASCII character instead.
   *
   * Identified by OpCode::PrintVariable. Represented by 7 bytes.
   *
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   * @param as_char If `true`, print least significant byte as char.
   */
  void printVariable(int32_t variable_index, bool follow_links, bool as_char);

  /**
   * @fn Program::setStringTableEntry
   * @brief Set the content of an entry in the string table
   *
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
   *
   * Sets the value that is stored for a specified entry in the string table.
   *
   * Identified by OpCode::SetStringTableEntry. Represented by 7 + string length bytes.
   *
   * @param string_table_index The string table index to set.
   * @param string The content to store in the string table.
   */
  void setStringTableEntry(int32_t string_table_index, const std::string& string);

  /**
   * @fn Program::printStringFromStringTable
   * @brief Prints a string from the string table
   *
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
   *
   * Identified by OpCode::PrintStringFromStringTable. Represented by 5 bytes.
   *
   * @param string_table_index The string stored at this index will be printed.
   */
  void printStringFromStringTable(int32_t string_table_index);

  /**
   * @fn Program::loadStringTableLimitIntoVariable
   * @brief Loads the maximum number of items in the string table into a variable
   *
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
   *
   * This is equivalent to the number of possible string items an associated VmSession provides.
   *
   * Identified by OpCode::LoadStringTableLimitIntoVariable. Represented by 6 bytes.
   *
   * @param variable_index The index of the variable to load the limit into.
   * @param follow_links Whether to resolve variable links.
   */
  void loadStringTableLimitIntoVariable(int32_t variable_index, bool follow_links);

  /**
   * @fn Program::terminate
   * @brief Immediately terminates the program
   *
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
   *
   * No further program operators will be executes. The program's return code will be set to the
   * value of the parameter `return_code`.
   *
   * Identified by OpCode::Terminate. Represented by 2 bytes.
   *
   * @param return_code The return code of the program after termination.
   */
  void terminate(int8_t return_code);

  /**
   * @fn Program::copyVariable
   * @brief Copies the value of one variable into another
   *
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
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
   * @brief Load the actual length of a string table item into a variable
   *
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
   *
   * Identified by OpCode::LoadStringItemLengthIntoVariable. Represented by 10 bytes.
   *
   * @param string_table_index The string table item's index to read the length from.
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   */
  void loadStringItemLengthIntoVariable(
      int32_t string_table_index, int32_t variable_index, bool follow_links);

  /**
   * @fn Program::checkIfInputWasSet
   * @brief Determine whether an input variable was written to since the last read operation
   *
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
   *
   * If since the last read operation the given variable at `variable_index` was written to, store
   * the value `0x1` in the variable at `destination_variable_index`. Store `0x0` otherwise.
   *
   * For details on I/O behaviors, also see VmSession::setVariableBehavior.
   *
   * Identified by OpCode::CheckIfInputWasSet. Represented by 11 bytes.
   *
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   * @param destination_variable_index The index of the destination variable.
   * @param follow_destination_links Whether to resolve destination variable links.
   * @sa VmSession::setVariableBehavior()
   */
  void checkIfInputWasSet(
      int32_t variable_index, bool follow_links,
      int32_t destination_variable_index, bool follow_destination_links);

  /**
   * @fn Program::loadStringTableItemLengthLimitIntoVariable
   * @brief Load the maximum length of string table items into a variable
   *
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
   *
   * This is the limit imposed on the Program by the VmSession associated to it during execution.
   *
   * Identified by OpCode::LoadStringTableItemLengthLimitIntoVariable. Represented by 6 bytes.
   *
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   */
  void loadStringTableItemLengthLimitIntoVariable(int32_t variable_index, bool follow_links);

  /**
   * @fn Program::loadRandomValueIntoVariable
   * @brief Stores a random value into the given variable
   *
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
   *
   * A random numeric value will be stored in the variable `variable_index`. It is drawn from the
   * full range of the data type `int32_t`.
   *
   * Identified by OpCode::LoadRandomValueIntoVariable. Represented by 6 bytes.
   *
   * @param variable_index The index of the variable to store the random value in.
   * @param follow_links Whether to resolve variable links.
   */
  void loadRandomValueIntoVariable(int32_t variable_index, bool follow_links);

  /**
   * @fn Program::unconditionalJumpToAbsoluteAddress
   * @brief Jumps to the given absolute address
   *
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
   *
   * Identified by OpCode::UnconditionalJumpToAbsoluteAddress. Represented by 5 bytes.
   *
   * @param addr The absolute address to jump to.
   */
  void unconditionalJumpToAbsoluteAddress(int32_t addr);

  /**
   * @fn Program::unconditionalJumpToAbsoluteVariableAddress
   * @brief Jumps to an absolute address stored in a variable
   *
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
   *
   * Identified by OpCode::UnconditionalJumpToAbsoluteVariableAddress. Represented by 6 bytes.
   *
   * @param variable_index The index of the variable holding the absolute address to jump to.
   * @param follow_links Whether to resolve variable links.
   */
  void unconditionalJumpToAbsoluteVariableAddress(int32_t variable_index, bool follow_links);

  /**
   * @fn Program::unconditionalJumpToRelativeAddress
   * @brief Jumps to the given relative address
   *
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
   *
   * Identified by OpCode::UnconditionalJumpToRelativeAddress. Represented by 5 bytes.
   *
   * @param addr The relativeaddress to jump to.
   */
  void unconditionalJumpToRelativeAddress(int32_t addr);

  /**
   * @fn Program::unconditionalJumpToRelativeVariableAddress
   * @brief Jumps to a relative address stored in a variable
   *
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
   *
   * Identified by OpCode::UnconditionalJumpToRelativeVariableAddress. Represented by 6 bytes.
   *
   * @param variable_index The index of the variable holding the relative address to jump to.
   * @param follow_links Whether to resolve variable links.
   */
  void unconditionalJumpToRelativeVariableAddress(int32_t variable_index, bool follow_links);

  /**
   * @fn Program::loadStringItemIntoVariables
   * @brief Loads an item from the string table into consecutive variables.
   *
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
   *
   * The item at the string table index `string_table_index` will be stored in the variables
   * starting at index `start_variable_index`, consecutively. Each variable stores the character
   * value from the respective string table item. Example:
   *
   * String table item: `Entry`
   * * `start_variable_index + 0 = E`
   * * `start_variable_index + 1 = n`
   * * `start_variable_index + 2 = t`
   * * `start_variable_index + 3 = r`
   * * `start_variable_index + 4 = y`
   *
   * The `follow_links` parameter will be applied to every index separately.
   *
   * Identified by OpCode::LoadStringItemIntoVariables. Represented by 10 bytes.
   *
   * @param string_table_index The string table item to read character from.
   * @param start_variable_index The index of the start variable.
   * @param follow_links Whether to resolve variable links.
   * @sa loadVariableStringItemLengthIntoVariable()
   */
  void loadStringItemIntoVariables(
      int32_t string_table_index, int32_t start_variable_index, bool follow_links);

  /**
   * @fn Program::performSystemCall
   * @brief Performs a system call according to its parameters
   *
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
   *
   * Performs system calls for different purposes. The currently implemented set of functionalities
   * and valid parameter ranges can be found in VmSession::performSystemCall.
   *
   * Identified by OpCode::PerformSystemCall. Represented by 8 bytes.
   *
   * @param major_code The major code of the system call to perform
   * @param minor_code The minor code of the system call to perform
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   * @sa VmSession::performSystemCall()
   */
  void performSystemCall(
      int8_t major_code, int8_t minor_code, int32_t variable_index, bool follow_links);

  /**
   * @fn Program::bitShiftVariableLeft
   * @brief Bit-shift a variable to the left
   *
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
   *
   * Bit-shifts the given variable by `places` bits to the left. The right side will be filled up
   * with zero bits.
   *
   * Identified by OpCode::BitShiftVariableLeft. Represented by 7 bytes.
   *
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   * @param places The amount of places to shift the variable to the left.
   */
  void bitShiftVariableLeft(int32_t variable_index, bool follow_links, int8_t places);

  /**
   * @fn Program::bitShiftVariableRight
   * @brief Bit-shift a variable to the right
   *
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
   *
   * Bit-shifts the given variable by `places` bits to the right. The left side will be filled up
   * with zero bits.
   *
   * Identified by OpCode::BitShiftVariableRight. Represented by 7 bytes.
   *
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   * @param places The amount of places to shift the variable to the right.
   */
  void bitShiftVariableRight(int32_t variable_index, bool follow_links, int8_t places);

  /**
   * @fn Program::bitWiseInvertVariable
   * @brief Bit-wise inverts the given variable
   *
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
   *
   * Identified by OpCode::BitWiseInvertVariable. Represented by 5 bytes.
   *
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   */
  void bitWiseInvertVariable(int32_t variable_index, bool follow_links);

  /**
   * @fn Program::bitWiseAndTwoVariables
   * @brief Performs a bit-wise AND operation on two variables
   *
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
   *
   * The result will be stored in `variable_index_b`.
   *
   * Identified by OpCode::BitWiseAndTwoVariables. Represented by 11 bytes.
   *
   * @param variable_index_a The index of the first variable.
   * @param follow_links_a Whether to resolve first variable's links.
   * @param variable_index_b The index of the second variable.
   * @param follow_links_b Whether to resolve second variable's links.
   */
  void bitWiseAndTwoVariables(
      int32_t variable_index_a, bool follow_links_a, int32_t variable_index_b, bool follow_links_b);

  /**
   * @fn Program::bitWiseOrTwoVariables
   * @brief Performs a bit-wise OR operation on two variables
   *
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
   *
   * The result will be stored in `variable_index_b`.
   *
   * Identified by OpCode::BitWiseOrTwoVariables. Represented by 11 bytes.
   *
   * @param variable_index_a The index of the first variable.
   * @param follow_links_a Whether to resolve first variable's links.
   * @param variable_index_b The index of the second variable.
   * @param follow_links_b Whether to resolve second variable's links.
   */
  void bitWiseOrTwoVariables(
      int32_t variable_index_a, bool follow_links_a, int32_t variable_index_b, bool follow_links_b);

  /**
   * @fn Program::bitWiseXorTwoVariables
   * @brief Performs a bit-wise XOR operation on two variables
   *
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
   *
   * The result will be stored in `variable_index_b`.
   *
   * Identified by OpCode::BitWiseXorTwoVariables. Represented by 11 bytes.
   *
   * @param variable_index_a The index of the first variable.
   * @param follow_links_a Whether to resolve first variable's links.
   * @param variable_index_b The index of the second variable.
   * @param follow_links_b Whether to resolve second variable's links.
   */
  void bitWiseXorTwoVariables(
      int32_t variable_index_a, bool follow_links_a, int32_t variable_index_b, bool follow_links_b);

  /**
   * @fn Program::moduloVariableByConstant
   * @brief Performs a constant modulo operation on a variable and stores the result
   *
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
   *
   * The given constant will be modulo'ed onto the given variable. The result will be stored in the
   * same variable.
   *
   * `variable = variable % constant`
   *
   * Identified by OpCode::ModuloVariableByConstant. Represented by 11 bytes.
   *
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   * @param modulo_constant The constant to modulo onto the variable.
   */
  void moduloVariableByConstant(
      int32_t variable_index, bool follow_links, int32_t modulo_constant);

  /**
   * @fn Program::moduloVariableByVariable
   * @brief Performs a variable modulo operation on a variable and stores the result
   *
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
   *
   * The modulo variable's value will be modulo'ed onto the given variable. The result will be
   * stored in the first variable.
   *
   * `variable = variable % modulo_variable`
   *
   * Identified by OpCode::ModuloVariableByVariable. Represented by 11 bytes.
   *
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   * @param modulo_variable_index The index of the modulo variable.
   * @param modulo_follow_links Whether to resolve the modulo variable's links.
   */
  void moduloVariableByVariable(
      int32_t variable_index, bool follow_links,
      int32_t modulo_variable_index, bool modulo_follow_links);

  /**
   * @fn Program::rotateVariableLeft
   * @brief Bit-wise rotate the given variable to the left
   *
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
   *
   * Negative values cause a rotation to the right.
   *
   * Identified by OpCode::RotateVariableLeft. Represented by 7 bytes.
   *
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   * @param places How many bits to rotate the variable to the left (can be negative).
   */
  void rotateVariableLeft(int32_t variable_index, bool follow_links, int8_t places);

  /**
   * @fn Program::rotateVariableRight
   * @brief Bit-wise rotate the given variable to the left
   *
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
   *
   * Negative values cause a rotation to the left.
   *
   * Identified by OpCode::RotateVariableRight. Represented by 7 bytes.
   *
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   * @param places How many bits to rotate the variable to the right (can be negative).
   */
  void rotateVariableRight(int32_t variable_index, bool follow_links, int8_t places);

  /**
   * @fn Program::pushVariableOnStack
   * @brief Pushes the content of a variable onto a stack
   *
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
   *
   * A stack is identified by a variable holding the current size of the stack, and a number of
   * declared variables after it. This operator increases the current size of the stack by 1, and
   * copies the value of the passed in variable into the variable index that is at
   * `stack_variable_index + previous_size + 1`.
   *
   * Identified by OpCode::PushVariableOnStack. Represented by 11 bytes.
   *
   * @param stack_variable_index The index of the stack variable.
   * @param follow_links_stack Whether to resolve the stack variable's links.
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   * @sa pushConstantOnStack(), popVariableFromStack(), popTopItemFromStack(), checkIfStackIsEmpty()
   */
  void pushVariableOnStack(
      int32_t stack_variable_index, bool follow_links_stack,
      int32_t variable_index, bool follow_links);

  /**
   * @fn Program::pushConstantOnStack
   * @brief Pushes a constant value onto a stack
   *
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
   *
   * Similar to pushVariableOnStack, but pushes a constant value passed to this operand onto the
   * stack rather than reading the value from a variable.
   *
   * Identified by OpCode::PushConstantOnStack. Represented by 10 bytes.
   *
   * @param stack_variable_index The index of the variable.
   * @param follow_links_stack Whether to resolve variable links.
   * @param constant tbd
   * @sa pushVariableOnStack(), popVariableFromStack(), popTopItemFromStack(), checkIfStackIsEmpty()
   */
  void pushConstantOnStack(
      int32_t stack_variable_index, bool follow_links_stack, int32_t constant);

  /**
   * @fn Program::popVariableFromStack
   * @brief Removes the top item from the stack and stores it in a variable.
   *
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
   *
   * Removes the top item from the stack, reducing the stack size by 1. The value of the removed
   * item will be stored in `variable_index`. Throws if the stack was empty before.
   *
   * Identified by OpCode::PopVariableFromStack. Represented by 11 bytes.
   *
   * @param stack_variable_index The index of the variable.
   * @param follow_links_stack Whether to resolve variable links.
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   * @sa pushVariableOnStack(), pushConstantOnStack(), popTopItemFromStack(), checkIfStackIsEmpty()
   */
  void popVariableFromStack(
      int32_t stack_variable_index, bool follow_links_stack,
      int32_t variable_index, bool follow_links);

  /**
   * @fn Program::popTopItemFromStack
   * @brief Removes the top item from the stack
   *
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
   *
   * Effectively, the stack size will be reduced by 1. The top item will be discarded. Throws if the
   * stack was empty before.
   *
   * Identified by OpCode::PopTopItemFromStack. Represented by 6 bytes.
   *
   * @param stack_variable_index The index of the variable.
   * @param follow_links_stack Whether to resolve variable links.
   * @sa popVariableFromStack(), popVariableFromStack(), checkIfStackIsEmpty()
   * @sa pushVariableOnStack(), pushConstantOnStack(), popVariableFromStack(), checkIfStackIsEmpty()
   */
  void popTopItemFromStack(int32_t stack_variable_index, bool follow_links_stack);

  /**
   * @fn Program::checkIfStackIsEmpty
   * @brief Checks whether there are no items on a stack
   *
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
   *
   * If the stack at variable `stack_variable_index` contains no items, the variable at
   * `variable_index` will be set to `0x1`, and `0x0` if there is at least one item on the stack.
   *
   * Identified by OpCode::CheckIfStackIsEmpty. Represented by 11 bytes.
   *
   * @param stack_variable_index The index of the variable.
   * @param follow_links_stack Whether to resolve variable links.
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   * @sa pushConstantOnStack(), pushVariableOnStack(), pushConstantOnStack(), popTopItemFromStack()
   */
  void checkIfStackIsEmpty(
      int32_t stack_variable_index, bool follow_links_stack,
      int32_t variable_index, bool follow_links);

  /**
   * @fn Program::swapVariables
   * @brief Swaps the contents of two variables
   *
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
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
   * @brief Sets an entry in the string table at a variable index
   *
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
   *
   * Identified by OpCode::SetVariableStringTableEntry. Represented by 8 + string length bytes.
   *
   * @param variable_index The index of the variable to read the string table index from.
   * @param follow_links Whether to resolve variable links.
   * @param string The string to store in the string table
   * @sa setStringTableEntry()
   */
  void setVariableStringTableEntry(
      int32_t variable_index, bool follow_links, const std::string& string);

  /**
   * @fn Program::printVariableStringFromStringTable
   * @brief Prints an entry from the string table from a variable index
   *
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
   *
   * Identified by OpCode::PrintVariableStringFromStringTable. Represented by 6 bytes.
   *
   * @param variable_index The index of the variable to read the string table index from.
   * @param follow_links Whether to resolve variable links.
   * @sa printStringFromStringTable()
   */
  void printVariableStringFromStringTable(int32_t variable_index, bool follow_links);

  /**
   * @fn Program::loadVariableStringItemLengthIntoVariable
   * @brief Loads the length of a variable string table index into a variable
   *
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
   *
   * Identified by OpCode::LoadVariableStringItemLengthIntoVariable. Represented by 11 bytes.
   *
   * @param string_item_variable_index The index of the variable to read the string table index from
   * @param follow_links_string_item Whether to resolve the string table index variable's links
   * @param variable_index The index of the variable to store the length in
   * @param follow_links Whether to resolve the target variable's links
   */
  void loadVariableStringItemLengthIntoVariable(
      int32_t string_item_variable_index, bool follow_links_string_item,
      int32_t variable_index, bool follow_links);

  /**
   * @fn Program::loadVariableStringItemIntoVariables
   * @brief Loads a variable item from the string table into consecutive variables.
   *
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
   *
   * See `loadStringItemLengthIntoVariable` for details. The string table index is not fixed in this
   * variant, but is read from the `string_item_variable_index` variable.
   *
   * Identified by OpCode::LoadVariableStringItemIntoVariables. Represented by 11 bytes.
   *
   * @param string_item_variable_index The index of the variable.
   * @param follow_links_string_item Whether to resolve variable links.
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   * @sa loadStringItemLengthIntoVariable()
   */
  void loadVariableStringItemIntoVariables(
      int32_t string_item_variable_index, bool follow_links_string_item,
      int32_t variable_index, bool follow_links);

  /**
   * @fn Program::terminateWithVariableReturnCode
   * @brief Immediately terminates the program, returning a variable return code
   *
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
   *
   * No further program operators will be executes. The program's return code will be set to the
   * value of the value of the `variable_index` variable.
   *
   * Identified by OpCode::TerminateWithVariableReturnCode. Represented by 6 bytes.
   *
   * @param variable_index The index of the variable.
   * @param follow_links Whether to resolve variable links.
   */
  void terminateWithVariableReturnCode(int32_t variable_index, bool follow_links);

  /**
   * @fn Program::variableBitShiftVariableLeft
   * @brief Bit-shifts a variable to the left by a variable amount of places
   *
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
   *
   * Identified by OpCode::VariableBitShiftVariableLeft. Represented by 11 bytes.
   *
   * @param variable_index The target variable that should be bit-shifted
   * @param follow_links Whether to resolve the target variable's links
   * @param places_variable_index The variable holding the amount to shift by
   * @param follow_links_places Whether to resolve the amount variable's links
   * @sa bitShiftVariableLeft()
   */
  void variableBitShiftVariableLeft(
      int32_t variable_index, bool follow_links,
      int32_t places_variable_index, bool follow_links_places);

  /**
   * @fn Program::variableBitShiftVariableRight
   * @brief Bit-shifts a variable to the right by a variable amount of places
   *
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
   *
   * Identified by OpCode::VariableBitShiftVariableRight. Represented by 11 bytes.
   *
   * @param variable_index The target variable that should be bit-shifted
   * @param follow_links Whether to resolve the target variable's links
   * @param places_variable_index The variable holding the amount to shift by
   * @param follow_links_places Whether to resolve the amount variable's links
   * @sa bitShiftVariableRight()
   */
  void variableBitShiftVariableRight(
      int32_t variable_index, bool follow_links,
      int32_t places_variable_index, bool follow_links_places);

  /**
   * @fn Program::variableRotateVariableLeft
   * @brief Rotates a variable to the left by a variable amount of places
   *
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
   *
   * Identified by OpCode::VariableRotateVariableLeft. Represented by 11 bytes.
   *
   * @param variable_index The target variable that should be rotated
   * @param follow_links Whether to resolve the target variable's links
   * @param places_variable_index The variable holding the amount to rotate by
   * @param follow_links_places Whether to resolve the amount variable's links
   */
  void variableRotateVariableLeft(
      int32_t variable_index, bool follow_links,
      int32_t places_variable_index, bool follow_links_places);

  /**
   * @fn Program::variableRotateVariableRight
   * @brief Rotates a variable to the right by a variable amount of places
   *
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
   *
   * Identified by OpCode::VariableRotateVariableRight. Represented by 11 bytes.
   *
   * @param variable_index The target variable that should be rotated
   * @param follow_links Whether to resolve the target variable's links
   * @param places_variable_index The variable holding the amount to rotate by
   * @param follow_links_places Whether to resolve the amount variable's links
   */
  void variableRotateVariableRight(
      int32_t variable_index, bool follow_links,
      int32_t places_variable_index, bool follow_links_places);

  /**
   * @fn Program::compareIfVariableGtConstant
   * @brief Compares if a variable is > a given constant
   *
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
   *
   * If the variable is greater than the given constant, the value `0x1` will be stored in the
   * target variable, else `0x0` will be stored.
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
   * @brief Compares if a variable is < a given constant
   *
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
   *
   * If the variable is smaller than the given constant, the value `0x1` will be stored in the
   * target variable, else `0x0` will be stored.
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
   * @brief Compares if a variable is = a given constant
   *
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
   *
   * If the variable is equal to the given constant, the value `0x1` will be stored in the target
   * variable, else `0x0` will be stored.
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
   * @brief Compares if a variable is > another variable
   *
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
   *
   * If variable a is greater than variable b, the value `0x1` will be stored in the target
   * variable, else `0x0` will be stored.
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
   * @brief Compares if a variable is < another variable
   *
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
   *
   * If variable a is smaller than variable b, the value `0x1` will be stored in the target
   * variable, else `0x0` will be stored.
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
   * @brief Compares if a variable is = another variable
   *
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
   *
   * If variable a is equal to variable b, the value `0x1` will be stored in the target variable,
   * else `0x0` will be stored.
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
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
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
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
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
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
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
   * This call adds the corresponding operator to the program. The operator's action does not take
   * immediate effect.
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
   * @brief Appends 4 bytes to the program's byte code store.
   *
   * Throws if the bytes don't fit for constant size programs. Extends the space for dynamically
   * growing programs if required.
   *
   * @param data The byte data to append to the program.
   */
  void appendData4(int32_t data);

  /**
   * @fn Program::appendData2
   * @brief Appends 2 bytes to the program's byte code store.
   *
   * Throws if the bytes don't fit for constant size programs. Extends the space for dynamically
   * growing programs if required.
   *
   * @param data The byte data to append to the program.
   */
  void appendData2(int16_t data);

  /**
   * @fn Program::appendData1
   * @brief Appends 1 byte to the program's byte code store.
   *
   * Throws if the byte doesn't fit for constant size programs. Extends the space for dynamically
   * growing programs if required.
   *
   * @param data The byte data to append to the program.
   */
  void appendData1(int8_t data);

  /**
   * @fn Program::appendFlag1
   * @brief Appends 1 byte to the program's byte code store representing a boolean flag
   *
   * If `flag` is `true`, the value `0x1` will be appended. Otherwise, `0x0` will be appended.
   *
   * Throws if the byte doesn't fit for constant size programs. Extends the space for dynamically
   * growing programs if required.
   *
   * @param flag The boolean flag to append to the program.
   */
  void appendFlag1(bool flag);

  /**
   * @fn Program::appendCode1
   * @brief Appends 1 byte to the program's byte code store representing an operator code
   *
   * The operator code will be converted into a 1 byte value and is then appended to the program
   * code.
   *
   * Throws if the byte doesn't fit for constant size programs. Extends the space for dynamically
   * growing programs if required.
   *
   * @param flag The boolean flag to append to the program.
   */
  void appendCode1(OpCode opcode);

  /**
   * @fn Program::ensureSize
   * @brief If too small, resizes the internal byte storage to accomodate up to `size` bytes.
   *
   * @param size The size that should be ensured to fit into the program space.
   */
  void ensureSize(uint32_t size);

  /**
   * @var Program::data_
   * @brief The internal program storage for byte code
   */
  std::vector<unsigned char> data_;

  /**
   * @var Program::pointer_
   * @brief Points to the next byte where any appended data will be stored
   */
  uint32_t pointer_;

  /**
   * @var Program::grows_dynamically_
   * @brief Denotes whether the program grows dynamically or is of constant size
   */
  bool grows_dynamically_;
};

}  // namespace beast

#endif  // BEAST_PROGRAM_HPP_
