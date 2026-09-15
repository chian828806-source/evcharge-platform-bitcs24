import DataVVue3 from '@kjgl77/datav-vue3';
import { createPinia } from 'pinia';
import { createApp } from 'vue';
import App from './App.vue';
import './styles/dashboard.css';

createApp(App).use(createPinia()).use(DataVVue3).mount('#app');
