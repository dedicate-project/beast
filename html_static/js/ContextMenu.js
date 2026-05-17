const {createElement : e} = React;
const {Menu, MenuItem, ListItemIcon, Divider} = MaterialUI;

export function ContextMenu({show, position, onClose, menuItems}) {
  const menuRef = React.useCallback((node) => {
    if (node !== null) {
      if (show) {
        const {x, y} = position;
        node.style.transform = `translate(${x}px, ${y}px)`;
      }
    }
  }, [ show, position ]);

  const handleClose = (e) => {
    e.stopPropagation();
    onClose();
  };

  return e(Menu, {
    ref : menuRef,
    anchorEl : menuRef.current,
    open : show,
    onClose : handleClose,
    style : {zIndex : 1500},
    transformOrigin : {
      vertical : "top",
      horizontal : "left",
    },
  },
           menuItems.map(
               (item, index) =>
                   item.isSeparator
                       ? e(Divider, {key : index})
                       : e(MenuItem, {
                           key : index,
                           disabled : item.disabled,
                           onClick : () => {
                             item.action();
                             onClose();
                           },
                         },
                           e(ListItemIcon, null, e("i", {className : "material-icons"}, item.icon)),
                           item.text)));
}
