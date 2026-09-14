<script setup lang="ts">
import { computed, onMounted, onUnmounted } from 'vue';
import { useDashboardStore } from './core/stores/dashboard';

const dashboard = useDashboardStore();
const apiStatus = computed(() => dashboard.overview.status);
const websocketStatus = computed(() => dashboard.realtime.connection.state);

onMounted(() => {
  void dashboard.refreshAll();
  dashboard.startRealtime();
});

onUnmounted(() => dashboard.stopRealtime());
</script>

<template>
  <main>
    <h1>Dashboard V2 Core loaded</h1>
    <p>data mode: {{ dashboard.dataMode }}</p>
    <p>API status: {{ apiStatus }}</p>
    <p>WebSocket status: {{ websocketStatus }}</p>
    <p v-if="dashboard.overview.error">API error: {{ dashboard.overview.error.message }}</p>
  </main>
</template>
