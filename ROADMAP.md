# BEAST Road Map

## Stage 1: Bytecode Interpreter (Virtual Machine)
- [x] Build the general pipeline for defining programs and running them
- [ ] Fully cover the entire code base
- [x] Add more operators to have a solid base of operators available
- [ ] Write up documentation for how things work, what the architecture is, etc.
- [x] Set up CI/CD using CircleCI, GitHub CodeQL, AppVeyor
- [x] Set up automatic build of Doxygen and Sphinx documentation using the ReadTheDocs service
- [x] Set up GitHub repository with all contributing guidelines, code of conduct, security guidelines, license, issue templates, etc.
- [ ] Clean up all code and documentation, finish documenting all entities
- [ ] Add more examples for how to use the virtual machine and implemented operators

## Stage 2: Genetic Algorithms and Program Evaluation
- [ ] Develop an environment where programs are semi-randomly generated and can be evaluated based on manually defined criteria
- [ ] Develop a genetic algorithms based approach to recombining programs that perform well based on the implemented criteria

## Stage 3: High level Language
- [ ] Develop a high level language that allows users to write BEAST programs in a script language
- [ ] Consider using Boost Spirit for parsing the high level language

## Stage 4: GPU Support
- [ ] Add GPU support for executing BEAST programs

## Future Stages
- [ ] TBD
