// Top-level error boundary wrapped around the entire App tree.
//
// React 18's production build silently unmounts the whole root subtree the moment any render
// throws -- which is exactly what happened with the original PipelineCanvas (TypeError on a
// null `metadata`). That left the user staring at a blank white page with no back button or
// debug context. This boundary catches such errors, keeps the chrome usable (back button,
// reload), and surfaces the actual exception so future regressions are visible immediately
// instead of presenting as a mysterious blank page.

const {createElement : e} = React;

export class ErrorBoundary extends React.Component {
  constructor(props) {
    super(props);
    this.state = {error : null, info : null};
  }

  static getDerivedStateFromError(error) { return {error}; }

  componentDidCatch(error, info) {
    this.setState({error, info});
    // eslint-disable-next-line no-console
    console.error('BEAST UI error:', error, info);
  }

  reset = () => { this.setState({error : null, info : null}); };

  render() {
    if (!this.state.error) {
      return this.props.children;
    }
    const message = this.state.error.message || String(this.state.error);
    const stack =
        (this.state.info && this.state.info.componentStack) || (this.state.error.stack || '');
    return e('div', {
      style : {
        position : 'fixed',
        top : 0,
        left : 0,
        right : 0,
        bottom : 0,
        padding : '24px',
        backgroundColor : '#fff8f8',
        color : '#222',
        fontFamily : 'system-ui, sans-serif',
        overflow : 'auto',
        zIndex : 100000,
      }
    },
             e('h2', {style : {marginTop : 0, color : '#c0392b'}}, 'Something went wrong.'),
             e('p', null, message),
             e('pre',
               {
                 style : {
                   whiteSpace : 'pre-wrap',
                   background : '#fff',
                   border : '1px solid #ddd',
                   padding : '12px',
                   borderRadius : '4px',
                   fontSize : '12px',
                   maxHeight : '40vh',
                   overflow : 'auto',
                 }
               },
               stack),
             e('div', {style : {marginTop : '16px'}},
               e('button',
                 {
                   onClick : this.reset,
                   style : {
                     padding : '8px 16px',
                     marginRight : '8px',
                     fontSize : '14px',
                     cursor : 'pointer',
                   }
                 },
                 'Try again'),
               e('button',
                 {
                   onClick : () => window.location.reload(),
                   style : {padding : '8px 16px', fontSize : '14px', cursor : 'pointer'}
                 },
                 'Reload page')));
  }
}
