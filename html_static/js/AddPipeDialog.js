// Modal for authoring a new pipe. Renders a type picker, then a per-type accordion of
// parameter sections derived from PIPE_TYPE_DEFINITIONS. On Save it builds the request
// body the backend's `add_pipe` action expects (see src/pipeline_server.cpp::handleAddPipe)
// and hands it back to the caller via `onSave`.

import {PIPE_TYPE_DEFINITIONS, defaultValuesFor, findPipeDefinition,
        valuesFromExistingPipe} from './PipeTypes.js';

const {useState, useMemo, createElement: e} = React;
const {
  Dialog,
  DialogTitle,
  DialogContent,
  DialogActions,
  Button,
  TextField,
  MenuItem,
  Checkbox,
  FormControlLabel,
  Accordion,
  AccordionSummary,
  AccordionDetails,
  Typography,
  Box,
} = MaterialUI;

// Material UI v4 ships ExpansionPanel as the actual component and aliases Accordion in
// some builds but not others; pick whichever is available.
const SafeAccordion = Accordion || MaterialUI.ExpansionPanel;
const SafeAccordionSummary = AccordionSummary || MaterialUI.ExpansionPanelSummary;
const SafeAccordionDetails = AccordionDetails || MaterialUI.ExpansionPanelDetails;

function coerceFieldValue(field, raw) {
  if (field.type === 'int') {
    const parsed = parseInt(raw, 10);
    return Number.isFinite(parsed) ? parsed : field.default;
  }
  if (field.type === 'float') {
    const parsed = parseFloat(raw);
    return Number.isFinite(parsed) ? parsed : field.default;
  }
  if (field.type === 'bool') {
    return Boolean(raw);
  }
  return raw;
}

function FieldInput({field, value, onChange}) {
  const handle = (event) => {
    const next = field.type === 'bool' ? event.target.checked : event.target.value;
    onChange(field.name, coerceFieldValue(field, next));
  };
  if (field.type === 'enum') {
    return e(TextField, {
      select: true,
      label: field.label,
      value: value !== undefined ? value : field.default,
      onChange: handle,
      fullWidth: true,
      margin: 'dense',
    }, field.options.map(opt => e(MenuItem, {key: opt.value, value: opt.value}, opt.label)));
  }
  if (field.type === 'bool') {
    return e(FormControlLabel, {
      control: e(Checkbox, {checked: Boolean(value), onChange: handle, color: 'primary'}),
      label: field.label,
    });
  }
  // int/float share the same numeric input; we use type="number" so mobile keyboards
  // surface the numeric pad too, and we round-trip through coerceFieldValue so that
  // even paste-in floats parse cleanly for int fields.
  return e(TextField, {
    label: field.label,
    type: 'number',
    value: value !== undefined && value !== null ? value : '',
    onChange: handle,
    fullWidth: true,
    margin: 'dense',
    inputProps: {
      min: field.min,
      max: field.max,
      step: field.step || (field.type === 'int' ? 1 : 'any'),
    },
  });
}

export function AddPipeDialog({
  open,
  onClose,
  onSave,
  existingPipeNames,
  anchorPosition,
  // When `editing` is set, the dialog turns into "Edit pipe parameters" mode: type and
  // name pickers are locked, the form is populated from the existing pipe's JSON, and the
  // Save button posts an `update_pipe_parameters` body instead of `add_pipe`. Shape:
  // `{ name: string, pipe_json: { type, parameters } }`.
  editing,
}) {
  const editingDefinition = useMemo(
      () => (editing && editing.pipe_json ? findPipeDefinition(editing.pipe_json) : null),
      [editing]);

  const initialType =
      (editingDefinition && editingDefinition.type) || PIPE_TYPE_DEFINITIONS[0].type;

  const [selectedType, setSelectedType] = useState(initialType);
  const [nameInput, setNameInput] = useState(editing ? editing.name : '');
  const [values, setValues] = useState(() => editing && editingDefinition
      ? valuesFromExistingPipe(editingDefinition, editing.pipe_json)
      : defaultValuesFor(PIPE_TYPE_DEFINITIONS[0]));
  const [submitting, setSubmitting] = useState(false);
  const [error, setError] = useState('');

  // Re-sync local state whenever the parent passes a new `editing` reference (e.g. user
  // clicks "Edit" on a different pipe). React preserves the same instance of this dialog
  // between opens since it's mounted always, so we have to react to prop changes
  // explicitly.
  React.useEffect(() => {
    if (editing && editingDefinition) {
      setSelectedType(editingDefinition.type);
      setNameInput(editing.name);
      setValues(valuesFromExistingPipe(editingDefinition, editing.pipe_json));
      setError('');
    }
  }, [editing, editingDefinition]);

  const definition = useMemo(
      () => PIPE_TYPE_DEFINITIONS.find(d => d.type === selectedType) || PIPE_TYPE_DEFINITIONS[0],
      [selectedType]);
  const isEditing = Boolean(editing);

  const handleTypeChange = (event) => {
    const nextType = event.target.value;
    const nextDef =
        PIPE_TYPE_DEFINITIONS.find(d => d.type === nextType) || PIPE_TYPE_DEFINITIONS[0];
    setSelectedType(nextType);
    setValues(defaultValuesFor(nextDef));
  };

  const handleFieldChange = (name, value) => {
    setValues(prev => ({...prev, [name]: value}));
  };

  const validate = () => {
    const trimmedName = nameInput.trim();
    if (!trimmedName) return 'Name is required.';
    if (!/^[A-Za-z][A-Za-z0-9_]*$/.test(trimmedName)) {
      // Names end up as object keys in the on-disk JSON and as identifiers in the canvas
      // tooltip; restricting them to identifier characters avoids escaping headaches and
      // keeps URLs clean too.
      return 'Name must start with a letter and contain only letters, digits, or underscores.';
    }
    // In edit mode the name field is locked to the existing pipe's name, so the duplicate
    // check would always trip. Skip it.
    if (!isEditing && existingPipeNames && existingPipeNames.indexOf(trimmedName) !== -1) {
      return `A pipe named "${trimmedName}" already exists.`;
    }
    return '';
  };

  const handleSave = async () => {
    const validationError = validate();
    if (validationError) {
      setError(validationError);
      return;
    }
    setError('');
    setSubmitting(true);
    try {
      const parameters = definition.buildParameters(values);
      const body = isEditing
        ? {action: 'update_pipe_parameters', name: nameInput.trim(), parameters}
        : {
            action: 'add_pipe',
            name: nameInput.trim(),
            type: definition.buildAsType || definition.type,
            parameters,
          };
      if (!isEditing && anchorPosition) {
        body.position = {x: anchorPosition.x | 0, y: anchorPosition.y | 0};
      }
      const ok = await onSave(body);
      if (ok && !isEditing) {
        // Reset for next invocation so we don't leak the previous form's name into the
        // duplicate-name validator. In edit mode we leave the values alone because the
        // dialog typically closes after a successful save.
        setNameInput('');
        setValues(defaultValuesFor(definition));
      }
    } finally {
      setSubmitting(false);
    }
  };

  const handleClose = () => {
    if (submitting) return;
    setError('');
    setNameInput('');
    setValues(defaultValuesFor(definition));
    onClose();
  };

  return e(Dialog, {open, onClose: handleClose, fullWidth: true, maxWidth: 'sm'},
           e(DialogTitle, null, isEditing ? `Edit pipe: ${editing.name}` : 'Add a new pipe'),
           e(DialogContent, null,
             e(TextField, {
               select: true,
               label: 'Pipe type',
               value: selectedType,
               onChange: handleTypeChange,
               fullWidth: true,
               margin: 'dense',
               disabled: isEditing,
               helperText: isEditing
                 ? 'Type cannot be changed after creation. Delete the pipe and re-add it to switch types.'
                 : undefined,
             }, PIPE_TYPE_DEFINITIONS.map(def =>
                                              e(MenuItem, {key: def.type, value: def.type},
                                                def.label))),
             e(Typography, {
               variant: 'body2',
               color: 'textSecondary',
               style: {marginTop: 4, marginBottom: 12}
             },
               definition.description),
             e(TextField, {
               label: 'Pipe name',
               value: nameInput,
               onChange: (event) => setNameInput(event.target.value),
               fullWidth: true,
               margin: 'dense',
               autoFocus: !isEditing,
               disabled: isEditing,
               helperText: isEditing
                 ? 'Renaming a pipe is not supported yet; use the canvas right-click menu instead.'
                 : 'Used as the JSON key and as the label on the canvas.',
             }),
             (definition.sections || []).map((section, sectionIdx) =>
                 e(SafeAccordion, {
                   key: section.title,
                   defaultExpanded: !section.collapsedByDefault,
                 },
                   e(SafeAccordionSummary, null,
                     e(Typography, {variant: 'subtitle2'}, section.title)),
                   e(SafeAccordionDetails, null,
                     e(Box, {style: {display: 'flex', flexDirection: 'column', width: '100%'}},
                       (section.fields || []).map(field =>
                           e('div', {key: field.name, style: {marginBottom: 4}},
                             e(FieldInput, {
                               field,
                               value: values[field.name],
                               onChange: handleFieldChange,
                             }))))))),
             error && e(Typography, {color: 'error', style: {marginTop: 12}}, error)),
           e(DialogActions, null,
             e(Button, {onClick: handleClose, disabled: submitting}, 'Cancel'),
             e(Button, {
               onClick: handleSave,
               disabled: submitting,
               color: 'primary',
               variant: 'contained',
             },
               submitting
                 ? (isEditing ? 'Saving…' : 'Adding…')
                 : (isEditing ? 'Save changes' : 'Add pipe'))));
}
