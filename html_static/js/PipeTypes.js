// Single source of truth for the pipe types the compose UI can author. Each entry knows
// how to render its own parameter form (`fields`), how to translate that form's values
// back into the on-wire JSON shape the backend expects (`buildParameters`), and how to
// describe itself visually on the pipeline canvas (`image`, `inputs`, `outputs`).
//
// Backed by the corresponding C++ branches of `PipelineManager::constructPipelineFromJson`
// in src/pipeline_manager.cpp; if you add a pipe type here you'll almost certainly want to
// extend that switch too (or the backend will reject the add_pipe action).

// Helper used by the EvaluatorPipe definition; pulled out so the form code can stay flat
// instead of growing one nested ternary per knob.
const evolutionParameterDefaults = {
  generations: 50,
  crossover_probability: 0.7,
  mutation_probability: 0.05,
  elitism: true,
  byte_mutation_share: 0.2,
  variable_count: 16,
  string_table_size: 4,
  string_table_item_length: 16,
  max_genome_bytes: 2048,
  starting_program_size: 0,
  opcode_weights: {},
};

export const PIPE_TYPE_DEFINITIONS = [
  {
    type: 'ProgramFactoryPipe',
    label: 'Program Factory',
    description: 'Emits random candidate programs for downstream pipes to evolve.',
    image: '/img/factory_pipe.png',
    inputs: 0,
    outputs: 1,
    sections: [
      {
        title: 'Factory',
        fields: [
          {
            name: 'factory',
            label: 'Program factory',
            type: 'enum',
            options: [{value: 'RandomProgramFactory', label: 'Random programs'}],
            default: 'RandomProgramFactory',
          },
          {name: 'max_candidates', label: 'Max candidates per cycle', type: 'int', default: 50, min: 1},
          {name: 'max_size', label: 'Max program size (bytes)', type: 'int', default: 80, min: 1},
        ],
      },
      {
        title: 'VM session',
        fields: [
          {name: 'memory_variables', label: 'Memory variables', type: 'int', default: 64, min: 1},
          {name: 'string_table_items', label: 'String table entries', type: 'int', default: 0, min: 0},
          {name: 'string_table_item_length', label: 'String table item length', type: 'int', default: 0, min: 0},
        ],
      },
    ],
    buildParameters: (values) => ({
      factory: values.factory,
      max_candidates: values.max_candidates,
      max_size: values.max_size,
      memory_variables: values.memory_variables,
      string_table_items: values.string_table_items,
      string_table_item_length: values.string_table_item_length,
    }),
    parseParameters: (params) => ({
      factory: params.factory,
      max_candidates: params.max_candidates,
      max_size: params.max_size,
      memory_variables: params.memory_variables,
      string_table_items: params.string_table_items,
      string_table_item_length: params.string_table_item_length,
    }),
  },
  {
    type: 'MazeEvaluatorPipe',
    label: 'Maze Evaluator',
    description: 'Wraps an EvaluatorPipe around a MazeEvaluator to evolve programs against a procedurally generated maze.',
    // What the backend actually sees: an EvaluatorPipe with a single MazeEvaluator child.
    // `buildAsType` is used by AddPipeDialog when POSTing; the canvas keeps using the
    // EvaluatorPipe-with-MazeEvaluator detection logic that's already in PipelineCanvas.js
    // to pick the maze icon for incoming pipes.
    buildAsType: 'EvaluatorPipe',
    image: '/img/maze_evaluator_pipe.png',
    inputs: 1,
    outputs: 1,
    sections: [
      {
        title: 'Pipe',
        fields: [
          {name: 'max_candidates', label: 'Max candidates per cycle', type: 'int', default: 20, min: 1},
          {name: 'memory_variables', label: 'Memory variables', type: 'int', default: 64, min: 1},
          {name: 'string_table_items', label: 'String table entries', type: 'int', default: 0, min: 0},
          {name: 'string_table_item_length', label: 'String table item length', type: 'int', default: 0, min: 0},
        ],
      },
      {
        title: 'Maze',
        fields: [
          {name: 'rows', label: 'Rows', type: 'int', default: 8, min: 3},
          {name: 'cols', label: 'Columns', type: 'int', default: 8, min: 3},
          {name: 'difficulty', label: 'Difficulty (0-1)', type: 'float', default: 0.1, min: 0.0, max: 1.0, step: 0.05},
          {name: 'max_steps', label: 'Max VM steps per evaluation', type: 'int', default: 2000, min: 1},
        ],
      },
      {
        title: 'Selection',
        fields: [
          {name: 'cut_off_score', label: 'Cut-off score', type: 'float', default: 0.0, min: 0.0, max: 1.0, step: 0.01},
        ],
      },
      {
        title: 'Genetic algorithm',
        // Collapsed by default since the defaults are usually fine; the dialog renders
        // sections as collapsible accordions.
        collapsedByDefault: true,
        fields: [
          {name: 'generations', label: 'Generations per cycle', type: 'int', default: evolutionParameterDefaults.generations, min: 1},
          {name: 'crossover_probability', label: 'Crossover probability', type: 'float', default: evolutionParameterDefaults.crossover_probability, min: 0.0, max: 1.0, step: 0.05},
          {name: 'mutation_probability', label: 'Mutation probability', type: 'float', default: evolutionParameterDefaults.mutation_probability, min: 0.0, max: 1.0, step: 0.01},
          {name: 'byte_mutation_share', label: 'Byte-level mutation share', type: 'float', default: evolutionParameterDefaults.byte_mutation_share, min: 0.0, max: 1.0, step: 0.05},
          {name: 'elitism', label: 'Elitism (keep best each gen)', type: 'bool', default: evolutionParameterDefaults.elitism},
          {name: 'starting_program_size', label: 'Starting program size (0 = factory default)', type: 'int', default: evolutionParameterDefaults.starting_program_size, min: 0},
          {name: 'max_genome_bytes', label: 'Max genome size (bytes)', type: 'int', default: evolutionParameterDefaults.max_genome_bytes, min: 1},
        ],
      },
    ],
    buildParameters: (values) => ({
      max_candidates: values.max_candidates,
      memory_variables: values.memory_variables,
      string_table_items: values.string_table_items,
      string_table_item_length: values.string_table_item_length,
      cut_off_score: values.cut_off_score,
      evaluators: [{
        type: 'MazeEvaluator',
        weight: 1.0,
        invert_logic: false,
        parameters: {
          rows: values.rows,
          cols: values.cols,
          difficulty: values.difficulty,
          max_steps: values.max_steps,
        },
      }],
      evolution_parameters: {
        ...evolutionParameterDefaults,
        generations: values.generations,
        crossover_probability: values.crossover_probability,
        mutation_probability: values.mutation_probability,
        byte_mutation_share: values.byte_mutation_share,
        elitism: values.elitism,
        starting_program_size: values.starting_program_size,
        max_genome_bytes: values.max_genome_bytes,
      },
    }),
    // The inverse of buildParameters; used by the EditPipeDialog when populating the form
    // from an existing pipe's JSON. Missing fields fall back to the schema defaults so an
    // older on-disk pipe that predates a knob still opens cleanly.
    parseParameters: (params) => {
      const maze = (params.evaluators && params.evaluators[0] && params.evaluators[0].parameters) || {};
      const ga = params.evolution_parameters || {};
      return {
        max_candidates: params.max_candidates,
        memory_variables: params.memory_variables,
        string_table_items: params.string_table_items,
        string_table_item_length: params.string_table_item_length,
        rows: maze.rows,
        cols: maze.cols,
        difficulty: maze.difficulty,
        max_steps: maze.max_steps,
        cut_off_score: params.cut_off_score,
        generations: ga.generations,
        crossover_probability: ga.crossover_probability,
        mutation_probability: ga.mutation_probability,
        byte_mutation_share: ga.byte_mutation_share,
        elitism: ga.elitism,
        starting_program_size: ga.starting_program_size,
        max_genome_bytes: ga.max_genome_bytes,
      };
    },
  },
  {
    type: 'NullSinkPipe',
    label: 'Null Sink',
    description: 'Consumes downstream output, throwing it away. Useful as the terminator of a pipeline.',
    image: '/img/null_sink_pipe.png',
    inputs: 1,
    outputs: 0,
    sections: [
      {
        title: 'Buffer',
        fields: [
          {name: 'max_candidates', label: 'Max candidates buffered', type: 'int', default: 100, min: 1},
        ],
      },
    ],
    buildParameters: (values) => ({max_candidates: values.max_candidates}),
    parseParameters: (params) => ({max_candidates: params.max_candidates}),
  },
  {
    type: 'MultiplexerPipe',
    label: 'Multiplexer',
    description:
      'Merges candidates from several input slots into a single output stream. Use to ' +
      'combine fresh candidates from a factory with survivors looped back from a ' +
      'downstream stage, or to fan multiple producers into one consumer.',
    image: '/img/multiplexer_pipe.png',
    // Port count is dynamic: the input slot count is set per-instance via the form below.
    // Output is always 1 (that's the whole point of a mux).
    inputs: (params) => Math.max(1, Math.min(16, Number(params.input_slots || 2))),
    outputs: 1,
    sections: [
      {
        title: 'Slots',
        fields: [
          {name: 'max_candidates', label: 'Max candidates per slot', type: 'int', default: 50, min: 1},
          {name: 'input_slots', label: 'Input slots (1-16)', type: 'int', default: 2, min: 1, max: 16},
        ],
      },
    ],
    buildParameters: (values) => ({
      max_candidates: values.max_candidates,
      input_slots: values.input_slots,
    }),
    parseParameters: (params) => ({
      max_candidates: params.max_candidates,
      input_slots: params.input_slots,
    }),
  },
  {
    type: 'DemultiplexerPipe',
    label: 'Demultiplexer',
    description:
      'Splits a single input stream across several output slots. Round-robin spreads work ' +
      'evenly across parallel downstream branches; broadcast copies every candidate to ' +
      'every output (useful when evaluating the same population against several tasks).',
    image: '/img/demultiplexer_pipe.png',
    inputs: 1,
    outputs: (params) => Math.max(1, Math.min(16, Number(params.output_slots || 2))),
    sections: [
      {
        title: 'Slots',
        fields: [
          {name: 'max_candidates', label: 'Max candidates per slot', type: 'int', default: 50, min: 1},
          {name: 'output_slots', label: 'Output slots (1-16)', type: 'int', default: 2, min: 1, max: 16},
          {
            name: 'strategy',
            label: 'Distribution strategy',
            type: 'enum',
            options: [
              {value: 'round_robin', label: 'Round-robin (one candidate per branch)'},
              {value: 'broadcast', label: 'Broadcast (every branch sees every candidate)'},
            ],
            default: 'round_robin',
          },
        ],
      },
    ],
    buildParameters: (values) => ({
      max_candidates: values.max_candidates,
      output_slots: values.output_slots,
      strategy: values.strategy,
    }),
    parseParameters: (params) => ({
      max_candidates: params.max_candidates,
      output_slots: params.output_slots,
      strategy: params.strategy,
    }),
  },
  {
    type: 'ResultsSummaryPipe',
    label: 'Results Summary',
    description:
      'Passthrough probe that reports score statistics (min/max/mean, best-ever, rolling ' +
      'window) for every candidate flowing through it. Put one right after an evaluator ' +
      'pipe to see how training is progressing, or between any two pipes to inspect ' +
      'intermediate scores.',
    image: '/img/results_summary_pipe.png',
    inputs: 1,
    outputs: 1,
    sections: [
      {
        title: 'Buffer',
        fields: [
          {name: 'max_candidates', label: 'Max candidates per cycle', type: 'int', default: 50, min: 1},
        ],
      },
      {
        title: 'Statistics',
        // Collapsed by default; the window-size default is good for most uses.
        collapsedByDefault: true,
        fields: [
          {name: 'window_size', label: 'Rolling window size (0 = default 256)', type: 'int', default: 0, min: 0},
        ],
      },
    ],
    buildParameters: (values) => ({
      max_candidates: values.max_candidates,
      window_size: values.window_size,
    }),
    parseParameters: (params) => ({
      max_candidates: params.max_candidates,
      window_size: params.window_size,
    }),
  },
];

// Build a flat values object for the form when editing a pipe. Falls back to the
// schema-level defaults for any field the pipe's JSON doesn't carry, so we never feed
// `undefined` into a number input.
export function valuesFromExistingPipe(definition, pipe_json) {
  const parsed = definition.parseParameters
    ? definition.parseParameters((pipe_json && pipe_json.parameters) || {})
    : {};
  const values = defaultValuesFor(definition);
  for (const key of Object.keys(parsed)) {
    if (parsed[key] !== undefined && parsed[key] !== null) {
      values[key] = parsed[key];
    }
  }
  return values;
}

// Map back from a pipe's on-wire type (and its parameters, for the EvaluatorPipe ->
// MazeEvaluatorPipe disambiguation) to the matching definition.
export function findPipeDefinition(pipe_json) {
  if (!pipe_json || !pipe_json.type) return null;
  if (pipe_json.type === 'EvaluatorPipe') {
    const evals = pipe_json.parameters && pipe_json.parameters.evaluators;
    if (evals && evals[0] && evals[0].type === 'MazeEvaluator') {
      return PIPE_TYPE_DEFINITIONS.find(def => def.type === 'MazeEvaluatorPipe');
    }
  }
  return PIPE_TYPE_DEFINITIONS.find(def => def.type === pipe_json.type);
}

// Resolve the input/output port counts for a pipe instance. Pipe types whose port count
// is fixed (e.g. NullSinkPipe is always 1 input / 0 outputs) just expose a number; types
// like MultiplexerPipe / DemultiplexerPipe whose ports depend on parameters expose a
// function that takes the pipe's parameters and returns the count. We default to 0 when
// nothing is configured so an unknown pipe still renders.
export function portCountsFor(definition, pipe_json) {
  if (!definition) return {inputs: 0, outputs: 0};
  const params = (pipe_json && pipe_json.parameters) || {};
  const resolve = (fieldOrFn) =>
    typeof fieldOrFn === 'function' ? Number(fieldOrFn(params) || 0) : Number(fieldOrFn || 0);
  return {
    inputs: resolve(definition.inputs),
    outputs: resolve(definition.outputs),
  };
}

// Default form values for a given definition; used both by AddPipeDialog when opening a
// fresh dialog and by EditPipeDialog as a fallback for missing keys.
export function defaultValuesFor(definition) {
  const values = {};
  for (const section of definition.sections || []) {
    for (const field of section.fields || []) {
      values[field.name] = field.default;
    }
  }
  return values;
}
