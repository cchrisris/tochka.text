#pragma once

#include <string>

namespace tochka {

inline std::string WorkProjection(long long me) {
  std::string me_lit = std::to_string(me);
  return "SELECT w.id, w.title, w.body, w.genre, w.cover_image, w.status, "
         "w.created_at::text AS created_at, "
         "u.id AS author_id, u.username AS author_username, "
         "u.display_name AS author_name, "
         "u.avatar_url AS author_avatar, "
         "(SELECT count(*) FROM likes l WHERE l.work_id = w.id) AS "
         "likes_count, "
         "(SELECT count(*) FROM comments c WHERE c.work_id = w.id "
         "AND c.status = 'visible') AS comments_count, "
         "EXISTS(SELECT 1 FROM likes l WHERE l.work_id = w.id "
         "AND l.user_id = " +
         me_lit +
         ") AS liked_by_me, "
         "COALESCE((SELECT array_agg(t.name ORDER BY t.name) FROM work_tags wt "
         "JOIN tags t ON t.id = wt.tag_id WHERE wt.work_id = w.id), '{}') AS "
         "tags "
         "FROM works w JOIN users u ON u.id = w.author_id ";
}

}  // namespace tochka
