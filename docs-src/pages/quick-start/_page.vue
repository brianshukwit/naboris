<template>
  <div class="container">
    <div class="columns">
      <div class="column is-10 is-offset-1">
        <section class="section">
          <nav class="breadcrumb" aria-label="breadcrumbs">
            <ul>
              <li>
                <nuxt-link to="/">
                  Home
                </nuxt-link>
              </li>
              <li>
                <nuxt-link to="/quick-start">
                  Quick Start
                </nuxt-link>
              </li>
              <li class="is-active">
                <nuxt-link :to="fullSlug">
                  {{ title }}
                </nuxt-link>
              </li>
            </ul>
          </nav>
        </section>
        <section class="section">
          <div class="content">
            <component :is="componentInstance" />
          </div>
        </section>
        <section class="section">
          <div class="columns">
            <div v-if="previousPage !== null" class="column">
              <a :href="previousPage.href">
                <i class="fas fa-arrow-left" />
                Previous:
                {{ previousPage.title }}
              </a>
            </div>
            <div v-if="nextPage !== null" class="column has-text-right">
              <a :href="nextPage.href">
                Next:
                {{ nextPage.title }}
                <i class="fas fa-arrow-right" />
              </a>
            </div>
          </div>
        </section>
      </div>
    </div>
  </div>
</template>
<script>
import installation from '~/content/docs/quick-start/installation.md';
import basicserver from '~/content/docs/quick-start/basic-server.md';
import serverconfig from '~/content/docs/quick-start/server-configuration.md';
import routing from '~/content/docs/quick-start/routing.md';
import staticfiles from '~/content/docs/quick-start/static-files.md';

const pages = {
  installation,
  'basic-server': basicserver,
  'server-configuration': serverconfig,
  routing,
  'static-files': staticfiles
};

export default {
  data () {
    const { page } = this.$route.params;
    const docs = pages[page];

    return {
      title: docs.attributes.title,
      page
    };
  },
  computed: {
    fullSlug () {
      return `/quick-start/${this.page}`;
    },
    componentInstance () {
      const docs = pages[this.page];
      return docs.vue.component;
    },
    previousPage () {
      const keys = Object.keys(pages);
      const prevIndex = keys
        .reduce((pIndex, key, i) => {
          if (key === this.page) {
            return i - 1;
          }
          return pIndex;
        }, -1);

      if (prevIndex >= 0) {
        const prevPage = keys[prevIndex];
        return {
          href: `/quick-start/${prevPage}`,
          title: pages[prevPage].attributes.title
        };
      }
      return null;
    },
    nextPage () {
      const keys = Object.keys(pages);
      const nextIndex = keys
        .reduce((nIndex, key, i, allKeys) => {
          if (key === this.page && allKeys.length > (i + 1)) {
            return i + 1;
          }
          return nIndex;
        }, -1);

      if (nextIndex >= 0) {
        const nextPage = keys[nextIndex];
        return {
          href: `/quick-start/${nextPage}`,
          title: pages[nextPage].attributes.title
        };
      }
      return null;
    }
  },
  created () {
  },
  head () {
    return {
      title: `naboris - ${this.title}`,
      meta: [
        { hid: 'description', name: 'description', content: `Get started quickly with ${this.title}.` }
      ]
    };
  }
};
</script>
