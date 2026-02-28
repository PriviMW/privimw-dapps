import React from 'react';
import ReactDOM from 'react-dom/client';
import App from './App';
import './App.css';

// NO React.StrictMode — it double-fires useEffect which causes
// duplicate Qt WebChannel connections
const root = ReactDOM.createRoot(document.getElementById('root')!);
root.render(<App />);
