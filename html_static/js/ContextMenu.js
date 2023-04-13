const {createElement : e} = React;
const {Menu, MenuItem} = MaterialUI;

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

  return e(
      Menu,
      {
        ref : menuRef,
        anchorEl : menuRef.current,
        open : show,
        onClose : onClose,
        style : {zIndex : 1500},
        transformOrigin : {
          vertical : 'top',
          horizontal : 'left',
        },
      },
      menuItems.map(
          (item, index) => e(MenuItem, {key : index, onClick : onClose}, item),
          ),
  );
}
