FROM node:20-alpine AS build

WORKDIR /app
COPY package.json ./
RUN npm install

COPY . .

# Пусто => фронт ходит на относительный /api того же origin (за nginx).
ARG VITE_API_URL=
ENV VITE_API_URL=$VITE_API_URL
RUN npm run build

FROM nginx:alpine
# Шаблон с ${DOMAIN} разворачивается entrypoint'ом nginx (envsubst) при старте.
# Ограничиваем подстановку только переменной DOMAIN, чтобы не задеть $host/$uri.
ENV NGINX_ENVSUBST_FILTER=DOMAIN
COPY default.conf.template /etc/nginx/templates/default.conf.template
COPY --from=build /app/dist /usr/share/nginx/html
EXPOSE 80 443
