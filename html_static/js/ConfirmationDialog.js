const {createElement : e} = React;
const {Dialog, DialogActions, DialogContent, DialogContentText, DialogTitle, Button} = MaterialUI;

export function ConfirmationDialog({open, onClose, onConfirm, title, content}) {
  return e(Dialog, {
    open : open,
    onClose : onClose,
  },
           e(DialogTitle, null, title), e(DialogContent, null, e(DialogContentText, null, content)),
           e(DialogActions, null, e(Button, {onClick : onClose, color : "primary"}, "No"),
             e(Button, {onClick : onConfirm, color : "secondary"}, "Yes")));
}
