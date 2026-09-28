import nextVitals from "eslint-config-next/core-web-vitals";

const config = [
  {
    ignores: ["dist/**", ".next/**", "release/**", "distributions/**", "desktop-releases/**"],
  },
  ...nextVitals,
  {
    rules: {
      "@next/next/no-html-link-for-pages": "off",
      "react-hooks/immutability": "off",
      "react-hooks/set-state-in-effect": "off",
      "react-hooks/static-components": "off",
    },
  },
];

export default config;
